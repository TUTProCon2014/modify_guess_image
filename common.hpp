#pragma once 

#include <algorithm>
#include <set>
#include <memory>
#include <vector>
#include <boost/optional.hpp>
#include <stack>
#include <opencv2/opencv.hpp>
#include <opencv/highgui.h>

#include "../utils/include/types.hpp"
#include "../utils/include/image.hpp"
#include "../utils/include/range.hpp"
#include "../utils/include/exception.hpp"


namespace procon { namespace modify {


cv::Scalar groupedColor[2] = {cv::Scalar(255, 0, 0), cv::Scalar(0, 255, 0)};

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


struct MatchingResult
{
    MatchingResult(size_t len, bool bMatch)
    : length(len), isMatched(bMatch){}


    operator bool() const { return isMatched; }

    size_t length;
    bool isMatched;
};


template <typename Iterator>
MatchingResult matchLeftClick(Iterator b, Iterator e)
{
    if(b == e || !(*b).isDownL())
        return MatchingResult(0, false);

    const auto lastIndex = (*b).index;
    if((++b) == e || !(*b).isUpL() || lastIndex != (*b).index)
        return MatchingResult(1, false);

    return MatchingResult(2, true);
}


template <typename Iterator>
MatchingResult matchLeftDrag(Iterator b, Iterator e)
{
    if(b == e || !(*b).isDownL())
        return MatchingResult(0, false);

    const auto lastIndex = (*b).index;
    if((++b) == e || !(*b).isUpL() || lastIndex == (*b).index)
        return MatchingResult(1, false);

    return MatchingResult(2, true);
}


template <typename Iterator>
MatchingResult matchLeftDoubleClick(Iterator b, Iterator e)
{
    auto ml1 = matchLeftClick(b, e);
    if(!ml1)
        return MatchingResult(ml1.length, false);

    const auto lastIndex = (*b).index;
    ++(++b);

    auto ml2 = matchLeftClick(b, e);
    if(ml2 && (*b).index == lastIndex)
        return MatchingResult(ml1.length + ml2.length, true);
    else
        return MatchingResult(ml1.length, false);
}


template <typename Iterator>
MatchingResult matchRightClick(Iterator b, Iterator e)
{
    if(b == e || !(*b).isDownR())
        return MatchingResult(0, false);

    const auto lastIndex = (*b).index;
    if((++b) == e || !(*b).isUpR() || lastIndex != (*b).index)
        return MatchingResult(1, false);

    return MatchingResult(2, true);
}


struct TileState
{
    TileState() : _state(0) {}

    bool isFree() const { return _state == 0; }
    bool isFixed() const { return _state == 1; }

    /** gidは0から始まる数字
    */
    size_t isGrouped() const { return _state > 1; }
    size_t isGrouped(size_t gid) const { return _state == gid + 2; }
    size_t groupId() const { return _state - 2; }

    void reset() { _state = 0; }
    void setFixed() { _state = 1; }
    void setGroup(size_t gid) { _state = gid + 2; }

  private:
    size_t _state;
};


//マウス操作のコールバック関数へ渡す引数用の構造体 
struct Parameter
{
    struct Saved
    {
        template <typename A, typename B>
        Saved(A&& idx, B&& tst)
        : index(std::forward<A>(idx)), tileState(std::forward<B>(tst))
        {}

        std::vector<std::vector<utils::ImageID>> index;
        std::vector<std::vector<TileState>> tileState;
    };


    Parameter(utils::DividedImage const & pb,
              std::vector<std::vector<utils::ImageID>> const & index,
              char const * title)
    : swpImage(pb.clone(), index),
      mouseEvSq(),
      tileState(std::vector<std::vector<TileState>>(pb.div_y(), std::vector<TileState>(pb.div_x(), TileState()))),
      windowName(title),
      _history(){}

    utils::SwappedImage swpImage;
    std::deque<MouseEvent> mouseEvSq;
    std::vector<std::vector<TileState>> tileState;
    char const * windowName;

    private: std::stack<Saved> _history;
    public:


    void swap_element(utils::Index2D const & idx1, utils::Index2D const & idx2)
    {
        swpImage.swap_element(idx1, idx2);
        std::swap(tileState[idx1[0]][idx1[1]], tileState[idx2[0]][idx2[1]]);
    }


    cv::Mat cvMat() const
    {
        auto dup = swpImage.clone();

        utils::DividedImage::foreach(swpImage, [&](size_t i, size_t j){
            if(tileState[i][j].isFixed()){
                dup.get_element(i, j).cvMat() *= 0.5;
                dup.get_element(i, j).cvMat() +=  cv::Scalar(0, 0, 255) * 0.5;
            }
            else if(tileState[i][j].isGrouped()){
                dup.get_element(i, j).cvMat() *= 0.5;
                dup.get_element(i, j).cvMat() +=  groupedColor[tileState[i][j].groupId()] * 0.5;
            }
        });

        return dup.cvMat();
    }


    void save()
    {
        _history.emplace(swpImage.get_index(), tileState);
    }


    void restore()
    {
        if (_history.empty()) return;

        auto& t = _history.top();
        swpImage = utils::SwappedImage(swpImage.dividedImage(), t.index);
        tileState = t.tileState;
        mouseEvSq.clear();
        _history.pop();
    }
};


}}