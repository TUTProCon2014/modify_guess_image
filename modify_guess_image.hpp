#pragma once

#include <opencv2/opencv.hpp>
#include <opencv/highgui.h>
#include <memory>
#include <future>
#include <algorithm>

#include "../inout/include/inout.hpp"
#include "../utils/include/types.hpp"
#include "../utils/include/image.hpp"
#include "../utils/include/range.hpp"
#include "../utils/include/exception.hpp"
#include "../guess_img/include/guess.hpp"
#include "common.hpp"
#include "interactive_guess.hpp"


namespace procon { namespace modify {


// マウス操作のコールバック
void Mouse(int event, int x, int y, int flags, void* param_) // コールバック関数
{
    // utils::writefln("(%, %)", x, y);

    static bool MOUSE_FLAG = false;
    // static int tate = -1, yoko = -1,tate2 = -1, yoko2 = -1;

    auto& param = *static_cast<Parameter*>(param_);
    auto& evSq = param.mouseEvSq;
    auto& img = param.swpImage;
    
    switch (event)
    {
      case CV_EVENT_LBUTTONUP:
      case CV_EVENT_LBUTTONDOWN:
      case CV_EVENT_RBUTTONUP:
      case CV_EVENT_RBUTTONDOWN:
        if(x >= 0 && y >= 0 && x < param.swpImage.width() && y < param.swpImage.height())
            evSq.emplace_back(event, utils::makeIndex2D(
                y / (img.height() / img.div_y()),
                x / (img.width() / img.div_x())));
        break;

      default:
        break;
    }

    utils::writefln("cmd size: %, cmds : %", evSq.size(), evSq);

    /**
    最長マッチ戦略でコマンド列を処理します。
    */
    size_t executeCmdN = 0;
    do{
        for (auto i : utils::iota(executeCmdN))
            evSq.pop_front();
        executeCmdN = matchLeftDrag(evSq.begin(), evSq.end());

        if(executeCmdN == 2){
            auto idx1 = evSq[0].index,
                 idx2 = evSq[1].index;

            if(idx1[0] > idx2[0]) std::swap(idx1[0], idx2[0]);
            if(idx1[1] > idx2[1]) std::swap(idx1[1], idx2[1]);

            for(auto r = idx1[0]; r <= idx2[0]; ++r)
                for (auto c = idx1[1]; c <= idx2[1]; ++c){
                    if (param.tileState[r][c].isFree())
                        param.tileState[r][c].setGroup(0);
                    else if(param.tileState[r][c].isGrouped())
                        param.tileState[r][c].setFixed();
                    else
                        param.tileState[r][c].reset();
                }

            continue;
        }

        executeCmdN = std::max(executeCmdN, matchLeftDoubleClick(evSq.begin(), evSq.end()));
        if(executeCmdN == 4){
            const auto idx1 = evSq[0].index;
            const auto r = idx1[0],
                       c = idx1[1];

            if (param.tileState[r][c].isFixed())
                param.tileState[r][c].reset();
            else
                param.tileState[r][c].setFixed();
            continue;
        }

        executeCmdN = std::max(executeCmdN, [](auto b, auto e){
            size_t dst = matchLeftClick(b, e);
            if(dst != 2)
                return dst;

            return dst + matchLeftClick(++(++b), e);
        }(evSq.begin(), evSq.end()));
        if(executeCmdN == 4){
            const auto idx1 = evSq[0].index,
                       idx2 = evSq[2].index;

            param.swap_element(idx1, idx2);
            continue;
        }

        if(executeCmdN){
            executeCmdN = evSq.size() == executeCmdN ? 0 : executeCmdN;
            continue;
        }

        executeCmdN = matchRightClick(evSq.begin(), evSq.end());
        if(executeCmdN == 2){
            auto idx1 = evSq[0].index,
                 idx2 = evSq[1].index;

            bool isRow = true;
            auto ir = utils::iota(0, 0, 1);
            ptrdiff_t di = 0;

            if(idx1 == idx2 && (idx1[0] == 0 || idx1[1] == 0)){
                isRow = idx1[0] == 0;
                ir = utils::iota(0, (isRow ? img.div_y() : img.div_x()) - 1, +1);
                di = +1;
            }
            else if(idx1 == idx2 && (idx1[0] == img.div_y() -1 || idx1[1] == img.div_x() - 1)){
                isRow = idx1[0] == img.div_y() - 1;
                ir = utils::iota((isRow ? img.div_y() : img.div_x()) - 1, 0, -1);
                di = -1;
            }
            else
                continue;

            if(executeCmdN){
                for(auto i: ir)
                    for(auto j: utils::iota(isRow ? img.div_x() : img.div_y())){
                        if(isRow) param.swap_element(utils::makeIndex2D(i, j), utils::makeIndex2D(i+di, j));
                        else      param.swap_element(utils::makeIndex2D(j, i), utils::makeIndex2D(j, i + di));
                    }
            }
            continue;
        }

        executeCmdN = executeCmdN == 0 ? (evSq.size() == 0 ? 0 : 1) : executeCmdN;
        executeCmdN = evSq.size() == executeCmdN ? 0 : executeCmdN;

    }while(executeCmdN);

    cv::imshow(param.windowName, param.cvMat());
}


/**
エンターを押せば、callbackが別スレッドで起動します
*/
template <typename Task>
std::vector<std::vector<utils::ImageID>> modify_guess_image(std::vector<std::vector<utils::ImageID>> const & before, utils::Problem const & pb, Task callback)
{
    const auto windowName = "Modify Guess Image";

    std::unique_ptr<Parameter> param(new Parameter(pb.dividedImage().clone(), before, windowName));

    std::vector<std::future<void>> sendingThreads;
    // boost::optional<std::shared_future<std::vector<std::vector<utils::Index2D>>>> guessThread = boost::none;

    cv::namedWindow(windowName, CV_WINDOW_AUTOSIZE);
    cv::imshow(windowName, param->cvMat());
    cv::setMouseCallback(windowName, Mouse, param.get());

    // 後続するスレッドを新たに作る
    auto spawnNewThread = [&](){
        sendingThreads.emplace_back(std::async(
            std::launch::async,
            callback,
            param->swpImage.get_index()));
    };


    // 画像推定を行う
    auto doInteractiveGuess = [&](){
        utils::collectException<std::runtime_error>([&](){
            auto pred = [](utils::Image const & a, utils::Image const & b, utils::Direction dir){ return guess::diff_connection(a, b, dir); };

            return interactive_guess(*param, pb, pred);
        })
        .onSuccess([&](std::vector<std::vector<utils::ImageID>>&& v){
            param->swpImage = utils::SwappedImage(param->swpImage.dividedImage(), v);

            utils::DividedImage::foreach(param->swpImage, [&](size_t i, size_t j){
                if(param->tileState[i][j].isGrouped())
                    param->tileState[i][j].reset();
            });
        })
        .onFailure([](std::runtime_error& ex){ utils::writeln(ex); });
    };


    // 後続するスレッドの状態を監視する
    auto arrangeThreads = [&](){
        std::vector<std::future<void>> newths;
        std::chrono::milliseconds span(0);  // 待たない

        for(auto& e: sendingThreads){
            auto state = e.wait_for(span);

            if(state != std::future_status::ready)
                newths.emplace_back(std::move(e));
        }
        sendingThreads = std::move(newths);
    };


    constexpr int enter = 10,
                  esc = 27,
                  space = 32;

    while(1){
        const int key = cv::waitKey(100);

        arrangeThreads();
        switch(key){
          case enter:
            spawnNewThread();
            break;

          case esc:
            goto Lreturn;

          case space:
            // if(!guessThread)
                doInteractiveGuess();
            // else
                // utils::writeln("now running a guess thread");
            break;

          default: {}
        }

        cv::imshow(windowName, param->cvMat());
    }

  Lreturn:

    return param->swpImage.get_index();
}

}}