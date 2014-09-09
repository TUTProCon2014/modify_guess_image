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
#include "interactive_guess.hpp"


namespace procon { namespace modify {


struct MouseEvent
{
    MouseEvent(int ev, utils::Index2D const & idx)
    : event(ev), index(idx) {}

    int event;
    utils::Index2D index;

    bool isUpL() const { return event == CV_EVENT_LBUTTONUP; }
    bool isDownL() const { return event == CV_EVENT_LBUTTONDOWN; }
    bool isUpR() const { return event == CV_EVENT_RBUTTONUP; }
    bool isDownR() const { return event == CV_EVENT_RBUTTONDOWN; }


    template <typename Stream>
    void to_string(Stream & os)
    {
        os << "MouseEvent(";
        if(isUpL()) os << "UL";
        else if(isUpR()) os << "UR";
        else if(isDownL()) os << "DL";
        else if(isDownR()) os << "DR";
        else os << "XX";
        os << ", ";

        utils::swriteOne(os, index);
        os << ")";
    }
};


template <typename Iterator>
size_t matchLeftClick(Iterator b, Iterator e)
{
    if(b == e || !(*b).isDownL())
        return 0;

    const auto lastIndex = (*b).index;
    if((++b) == e || !(*b).isUpL() || lastIndex != (*b).index)
        return 1;

    return 2;
}


template <typename Iterator>
size_t matchLeftDrag(Iterator b, Iterator e)
{
    if(b == e || !(*b).isDownL())
        return 0;

    const auto lastIndex = (*b).index;
    if((++b) == e || !(*b).isUpL() || lastIndex == (*b).index)
        return 1;

    return 2;
}


template <typename Iterator>
size_t matchLeftDoubleClick(Iterator b, Iterator e)
{
    size_t ret = matchLeftClick(b, e);
    if(ret != 2)
        return ret;

    const auto lastIndex = (*b).index;
    ++(++b);

    ret += matchLeftClick(b, e);
    if(ret != 2 && (*b).index == lastIndex)
        return ret;
    else
        return 2;
}


template <typename Iterator>
size_t matchRightClick(Iterator b, Iterator e)
{
    if(b == e || !(*b).isDownR())
        return 0;

    const auto lastIndex = (*b).index;
    if((++b) == e || !(*b).isUpR() || lastIndex != (*b).index)
        return 1;

    return 2;
}


//マウス操作のコールバック関数へ渡す引数用の構造体 
struct Parameter
{
    Parameter(utils::DividedImage const & pb,
              std::vector<std::vector<utils::Index2D>> const & index,
              char const * title)
    : swpImage(pb.clone(), index), mouseEvSq(),
      isFixed(std::vector<std::vector<bool>>(pb.div_y(), std::vector<bool>(pb.div_x(), false))),
	  windowName(title){}

    utils::SwappedImage swpImage;
    std::deque<MouseEvent> mouseEvSq;
    std::vector<std::vector<bool>> isFixed;
    char const * windowName;


    void swap_element(utils::Index2D const & idx1, utils::Index2D const & idx2)
    {
        swpImage.swap_element(idx1, idx2);
        std::swap(isFixed[idx1[0]][idx1[1]], isFixed[idx2[0]][idx2[1]]);
    }


    cv::Mat cvMat() const
    {
        auto dup = swpImage.clone();

        utils::DividedImage::foreach(swpImage, [&](size_t i, size_t j){
            if(isFixed[i][j]){
                dup.get_element(i, j).cvMat() *= 0.5;
                dup.get_element(i, j).cvMat() +=  cv::Scalar(0, 0, 255) * 0.5;
            }
        });

        return dup.cvMat();
    }
};



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

    // utils::writefln("cmd size: %, cmds : %", evSq.size(), evSq);

    /**
    最長マッチ戦略でコマンド列を処理します。
    たとえば、
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
                    param.isFixed[r][c] = !param.isFixed[r][c];
                    // utils::writefln("(%, %)", r, c);
                }

            continue;
        }

        executeCmdN = std::max(executeCmdN, matchLeftDoubleClick(evSq.begin(), evSq.end()));
        if(executeCmdN == 4){
            const auto idx1 = evSq[0].index,
                       idx2 = evSq[2].index;

            param.isFixed[idx1[0]][idx1[1]] = !param.isFixed[idx1[0]][idx1[1]];
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
std::vector<std::vector<utils::Index2D>> modify_guess_image(std::vector<std::vector<utils::Index2D>> const & before, utils::Problem const & pb, Task callback)
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

            std::vector<std::vector<boost::optional<utils::Index2D>>> opIndex(pb.div_y());
            auto& index = param->swpImage.get_index();
            utils::DividedImage::foreach(pb, [&opIndex, &index, &param](std::size_t i, std::size_t j){
                if(param->isFixed[i][j])
                    opIndex[i].emplace_back(index[i][j]);
                else
                    opIndex[i].emplace_back(boost::none);
            });

            return interactive_guess(opIndex, pb, pred);
        })
        .onSuccess([&](std::vector<std::vector<Index2D>>&& v){
            param->swpImage = utils::SwappedImage(param->swpImage.dividedImage(), v);
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