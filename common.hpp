#pragma once 

#include <algorithm>
#include <set>
#include <memory>
#include <vector>
#include <boost/optional.hpp>

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
    Parameter(utils::DividedImage const & pb,
              std::vector<std::vector<utils::ImageID>> const & index,
              char const * title)
    : swpImage(pb.clone(), index),
	  mouseEvSq(),
      tileState(std::vector<std::vector<TileState>>(pb.div_y(), std::vector<TileState>(pb.div_x(), TileState()))),
      windowName(title){}

    utils::SwappedImage swpImage;
    std::deque<MouseEvent> mouseEvSq;
    std::vector<std::vector<TileState>> tileState;
    char const * windowName;


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
};

}}