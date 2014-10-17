#pragma once

#include <opencv2/opencv.hpp>
#include <opencv/highgui.h>
#include <memory>
#include <future>
#include <algorithm>
#include <stack>

#include "../inout/include/inout.hpp"
#include "../utils/include/types.hpp"
#include "../utils/include/image.hpp"
#include "../utils/include/range.hpp"
#include "../utils/include/exception.hpp"
#include "../guess_img/include/guess.hpp"
#include "../guess_img/include/correlation.hpp"
#include "../guess_img/include/correlation_s.hpp"
#include "common.hpp"
#include "interactive_guess.hpp"


namespace procon { namespace modify {



void onRegionSelected(Parameter& param, Index2D const & idx1, Index2D const & idx2)
{
    for(auto r = idx1[0]; r <= idx2[0]; ++r)
        for (auto c = idx1[1]; c <= idx2[1]; ++c){
            if (param.tileState[r][c].isFree())
                param.tileState[r][c].setGroup(0);
            else if(param.tileState[r][c].isGrouped())
                param.tileState[r][c].setFixed();
            else
                param.tileState[r][c].reset();
        }
}


void onRegionSelectedByRight(Parameter& param, Index2D const & idx1, Index2D const & idx2)
{
    for(auto r = idx1[0]; r <= idx2[0]; ++r)
        for (auto c = idx1[1]; c <= idx2[1]; ++c)
            if(!param.tileState[r][c].isFixed())
                param.tileState[r][c].setFixed();
}


void onLeftDoubleClick(Parameter& param, Index2D const & idx1)
{
    const auto r = idx1[0],
               c = idx1[1];

    if (param.tileState[r][c].isFixed())
        param.tileState[r][c].reset();
    else
        param.tileState[r][c].setFixed();
}


void onSelect2Tile(Parameter& param, Index2D const & idx1, Index2D const & idx2)
{
    param.swap_element(idx1, idx2);
}


void onRightClickEdge(Parameter& param, Index2D const & idx1)
{
    auto& img = param.swpImage;
    bool isRow = true;
    auto ir = utils::iota(0, 0, 1);
    ptrdiff_t di = 0;

    if(idx1[0] == 0 || idx1[1] == 0){
        isRow = idx1[0] == 0;
        ir = utils::iota(0, (isRow ? img.div_y() : img.div_x()) - 1, +1);
        di = +1;
    }
    else if(idx1[0] == img.div_y() -1 || idx1[1] == img.div_x() - 1){
        isRow = idx1[0] == img.div_y() - 1;
        ir = utils::iota((isRow ? img.div_y() : img.div_x()) - 1, 0, -1);
        di = -1;
    }else
        PROCON_ENFORCE(false, "logic error");

    for(auto i: ir)
        for(auto j: utils::iota(isRow ? img.div_x() : img.div_y())){
            if(isRow) param.swap_element(utils::makeIndex2D(i, j), utils::makeIndex2D(i+di, j));
            else      param.swap_element(utils::makeIndex2D(j, i), utils::makeIndex2D(j, i + di));
        }
}


// マウス操作のコールバック
void Mouse(int event, int x, int y, int flags, void* param_) // コールバック関数
{
    auto& param = *static_cast<Parameter*>(param_);
    auto& evSq = param.mouseEvSq;
    auto& img = param.swpImage;
    
    switch (event)
    {
      case CV_EVENT_LBUTTONUP:
      case CV_EVENT_LBUTTONDOWN:
      case CV_EVENT_RBUTTONUP:
      case CV_EVENT_RBUTTONDOWN:
        if(x >= 0 && y >= 0 && opCmp<size_t>(x, param.swpImage.width()) < 0 && opCmp<size_t>(y, param.swpImage.height()) < 0)
            evSq.emplace_back(event, utils::makeIndex2D(
                y / (img.height() / img.div_y()),
                x / (img.width() / img.div_x())));
        break;

      default:
        break;
    }

    //utils::writefln("cmd size: %, cmds : %", evSq.size(), evSq);

    auto consumeN = [&](size_t n){
        for(size_t i = 0; i < n; ++i)
            evSq.pop_front();
    };

    /**
    最長マッチ戦略でコマンド列を処理します。
    */
    while(!evSq.empty()){
        size_t matchLength = 0;

        auto m1 = matchLeftDrag(evSq.begin(), evSq.end());
        if(m1){
            param.save();

            auto idx1 = evSq[0].index,
                 idx2 = evSq[1].index;

            if(idx1[0] > idx2[0]) std::swap(idx1[0], idx2[0]);
            if(idx1[1] > idx2[1]) std::swap(idx1[1], idx2[1]);

            onRegionSelected(param, idx1, idx2);
            consumeN(m1.length);
            continue;
        }else
            matchLength = m1.length;


        MatchingResult m2 = matchLeftDoubleClick(evSq.begin(), evSq.end());
        if(m2){
            param.save();
            const auto idx1 = evSq[0].index;

            onLeftDoubleClick(param, idx1);
            consumeN(m2.length);
            continue;
        }else
            matchLength = std::max(matchLength, m2.length);

        auto m3 = [](decltype(evSq.begin()) b, decltype(evSq.end()) e) -> MatchingResult {
            auto mm1 = matchLeftClick(b, e);
            if(mm1){
                ++b; ++b;
                auto mm2 = matchLeftClick(b, e);
                if(mm2)
                    return MatchingResult(mm1.length + mm2.length, true);
                else
                    return MatchingResult(mm1.length + mm2.length, false);
            }else
                return MatchingResult(mm1.length, false);
        }(evSq.begin(), evSq.end());

        if(m3){
            param.save();
            const auto idx1 = evSq[0].index,
                       idx2 = evSq[2].index;

            onSelect2Tile(param, idx1, idx2);
            consumeN(m3.length);
            continue;
        }

        matchLength = std::max(matchLength, m3.length);


        auto m4 = matchRightClick(evSq.begin(), evSq.end());
        if(m4){
            auto idx1 = evSq[0].index;

            if(idx1[0] == 0 || idx1[1] == 0 || idx1[0] == img.div_y() - 1 || idx1[1] == img.div_x() - 1){
                param.save();
                onRightClickEdge(param, idx1);
                consumeN(m4.length);
                continue;
            }
        }

        matchLength = std::max(matchLength, m4.length);


        auto m5 = matchRightDrag(evSq.begin(), evSq.end());
        if(m5){
            param.save();

            auto idx1 = evSq[0].index,
                 idx2 = evSq[1].index;

            if(idx1[0] > idx2[0]) std::swap(idx1[0], idx2[0]);
            if(idx1[1] > idx2[1]) std::swap(idx1[1], idx2[1]);

            onRegionSelectedByRight(param, idx1, idx2);
            consumeN(m5.length);
            continue;
        }

        matchLength = std::max(matchLength, m5.length);


        if(matchLength < evSq.size())
            consumeN(std::max(static_cast<size_t>(1), matchLength));
        else if(matchLength == evSq.size())
            break;
    }

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
    auto doInteractiveGuess = [&param, &pb](const auto& pred){
        utils::collectException<std::runtime_error>([&](){
            return interactive_guess(*param, pb, pred);
        })
        .onSuccess([&](std::vector<std::vector<utils::ImageID>>&& v){
            param->save();
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


    constexpr int enter10 = 10,
                  enter13 = 13,
                  esc = 27,
                  space = 32,
                  key_z = 97 + 'z' - 'a',
                  key_c = 97 + 'c' - 'a',
                  tab = 9;

    auto pred_guess = guess::Correlator(pb);
    auto pred_s = guess_s::Correlator(pb);

    while(1){
        const int key = cv::waitKey(100);

        arrangeThreads();
        switch(key){
          case enter10:
          case enter13:
            spawnNewThread();
            break;

          case esc:
            goto Lreturn;

          case space:
            // if(!guessThread)
                doInteractiveGuess(pred_guess);
            // else
                // utils::writeln("now running a guess thread");
            break;

          case key_z:
            param->restore();
            break;

          case key_c:
            param->mouseEvSq.clear();
            param->save();
            utils::DividedImage::foreach(param->swpImage, [&](size_t i, size_t j){
                param->tileState[i][j].reset();
            });
            break;

          case tab:
            doInteractiveGuess(pred_s);

          default: {}
        }

        cv::imshow(windowName, param->cvMat());
    }

  Lreturn:
    cv::destroyWindow(windowName);
    return param->swpImage.get_index();
}

}}