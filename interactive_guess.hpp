#pragma once

#include <algorithm>
#include <set>
#include <memory>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <boost/optional.hpp>
#include <boost/range/adaptors.hpp>

#include "../utils/include/types.hpp"
#include "../utils/include/image.hpp"
#include "../utils/include/range.hpp"
#include "../utils/include/exception.hpp"
#include "../utils/include/dwrite.hpp"
#include "common.hpp"

namespace procon { namespace modify {

using namespace utils;

using Group = std::vector<std::tuple<ImageID, std::array<std::ptrdiff_t, 2>>>;
using OptionalMap = std::vector<std::vector<boost::optional<ImageID>>>;
using ImgMap = std::vector<std::vector<ImageID>>;
using Remains = std::unordered_set<ImageID>;


template <typename BinFunc>
double calcAllValue(ImgMap const & imgMap, Problem const & pb, BinFunc pred)
{
    double sumV = 0;
    for (auto i : iota(1, pb.div_y()))
        for (auto j : iota(0, pb.div_x())){
            const auto imgID1 = imgMap[i-1][j],
                       imgID2 = imgMap[i][j];

            sumV += pred(pb.get_element(imgID1), pb.get_element(imgID2), Direction::down);
        }

    for (auto i : iota(0, pb.div_y()))
        for (auto j : iota(1, pb.div_x())){
            const auto imgID1 = imgMap[i][j - 1],
                       imgID2 = imgMap[i][j];
        
            sumV += pred(pb.get_element(imgID1), pb.get_element(imgID2), Direction::right);
        }

    return sumV;
}


template <typename BinFunc>
ImgMap fill_remain_tile(
        OptionalMap const & imgMap,
        Remains const & remain,
        Problem const & pb,
        BinFunc pred)
{
    OptionalMap before = imgMap;

    PROCON_ENFORCE(before.size() == pb.div_y(), "Contract error: 'before.size() != pb.div_y()'");

    for(auto i: iota(pb.div_y()))
        PROCON_ENFORCE(before[i].size() == pb.div_x(), format("Contract error: 'before[%].size() != pb.div_x()'", i));

    PROCON_ENFORCE(remain.size() < pb.div_x() * pb.div_y(), "Contract error: 'all argument(before)'s elements are null.");

    // beforeでの抜け落ち`where`の周囲について、`which`画像がどの程度マッチするかを返す
    auto around_pred_value = [&](Index2D where, ImageID which){
        const auto i = where[0],
                   j = where[1];

        auto pred_value = [&](ImageID a, ImageID b, Direction dir)
        { return std::abs(pred(pb.get_element(a), pb.get_element(b), dir)); };

        double v = 0;
        if(i > 0 && !!before[i-1][j])               v += pred_value(which, *before[i-1][j], Direction::up);
        if(i < pb.div_y() - 1 && !!before[i+1][j])  v += pred_value(which, *before[i+1][j], Direction::down); 
        if(j > 0 && !!before[i][j-1])               v += pred_value(which, *before[i][j-1], Direction::left);
        if(j < pb.div_x() - 1 && !!before[i][j+1])  v += pred_value(which, *before[i][j+1], Direction::right);

        // writefln("%, %, %", where, which, v);
        return v;
    };


    // 次のbeforeでの抜け落ち位置を返す
    auto get_nextTargetIndex = [&](){
        auto targetIndex = boost::optional<Index2D>(boost::none);
        std::size_t maxN = 0;
        for(auto i: iota(pb.div_y()))
            for(auto j: iota(pb.div_x())){
                if(before[i][j])
                    continue;

                std::size_t cnt = 0;
                if(i > 0 && !!before[i-1][j])                   ++cnt;
                if(i < pb.div_y() - 1 && !!before[i+1][j])      ++cnt;
                if(j > 0 && !!before[i][j-1])                   ++cnt;
                if(j < pb.div_x() - 1 && !!before[i][j+1])      ++cnt;

                if(maxN < cnt){
                    maxN = cnt;
                    targetIndex = makeIndex2D(i, j);

                    if(cnt == 4) goto Lreturn;
                }
            }
      Lreturn:
        return targetIndex;
    };


    Remains rem = remain;
    while(1){
        auto tgtIdx = get_nextTargetIndex();
        if (!tgtIdx)
            break;

        PROCON_ENFORCE(!before[(*tgtIdx)[0]][(*tgtIdx)[1]], "Error");

        // writeln(*tgtIdx);
        PROCON_ENFORCE(!rem.empty(), "Error, rem.empty() == true");

        double min = std::numeric_limits<double>::infinity();
        boost::optional<ImageID> mostImg = boost::none;
        for(auto& imgId: rem){
            const double v = around_pred_value(*tgtIdx, imgId);
            if(v <= min){
                min = v;
                mostImg = imgId;
            }
        }

        // writeln(*mostIndex);
        PROCON_ENFORCE(mostImg, "Error, mostImg is null");
        before[(*tgtIdx)[0]][(*tgtIdx)[1]] = *mostImg;
        rem.erase(*mostImg);
    }

    ImgMap dst; dst.reserve(before.size());
    for(auto i: iota(before.size())){
        dst.emplace_back(before[i].size());
        for(auto j: iota(before[i].size()))
            dst[i][j] = *PROCON_ENFORCE(before[i][j], "Error: all before's elements are null.");
    }

    // writeln(dst);
    return dst;
}


bool is_fit(Group const & group, OptionalMap const & imgMap, std::size_t i, std::size_t j)
{
    if(i >= imgMap.size() || j >= imgMap[i].size())
        return false;

    auto can_put = [&](std::ptrdiff_t i, std::ptrdiff_t j) -> bool {
        if(opCmp<ptrdiff_t>(i, imgMap.size()) >= 0 || opCmp<ptrdiff_t>(j, imgMap[i].size()) >= 0 || i < 0 || j < 0) return false;
        return !imgMap[i][j];
    };

    for(auto& e: group)
        if(!can_put(static_cast<std::ptrdiff_t>(i) + std::get<1>(e)[0], static_cast<std::ptrdiff_t>(j) + std::get<1>(e)[1]))
            return false;

    return true;
}


void set_opt_map(Group const & g, OptionalMap& imgMap, size_t i, size_t j)
{
    for (auto& e : g)
        imgMap[i + std::get<1>(e)[0]][j + std::get<1>(e)[1]] = std::get<0>(e);
}

void reset_opt_map(Group const & g, OptionalMap& imgMap, size_t i, size_t j)
{
    for (auto& e : g)
        imgMap[i + std::get<1>(e)[0]][j + std::get<1>(e)[1]] = boost::none;
}


template <typename Iter, typename BinFunc>
std::tuple<double, ImgMap>
    position_bfs(Iter bg, Iter ed,
                 OptionalMap const & imgMap,
                 Remains const & remain,
                 Problem const & pb,
                 BinFunc pred)
{
    OptionalMap copyedMap = imgMap;

    if (bg == ed){
        auto idxArr = fill_remain_tile(copyedMap, remain, pb, pred);
        double val = calcAllValue(idxArr, pb, pred);

        return std::make_tuple(val, idxArr);
    }

    Group& g = *bg;
    Iter next = bg + 1;

    auto dst = std::make_tuple(std::numeric_limits<double>::infinity(), ImgMap());
    DividedImage::foreach(pb, [&](size_t i, size_t j){
        if(is_fit(g, copyedMap, i, j)){
            set_opt_map(g, copyedMap, i, j);
            auto res = position_bfs(next, ed, copyedMap, remain, pb, pred);
            reset_opt_map(g, copyedMap, i, j);

            if(std::get<0>(res) <= std::get<0>(dst))
                dst = std::move(res);
        }
    });

    return dst;
}


template <typename BinFunc>
ImgMap interactive_guess(Parameter const & param, Problem const & pb, BinFunc pred)
{
    using namespace boost::adaptors;

    auto& imgIdx = param.swpImage.get_index();
    auto gps = [&](){
        std::vector<Group> gps;
        DividedImage::foreach(param.swpImage, [&](size_t i, size_t j){
            if(param.tileState[i][j].isGrouped()){
                const auto gId = param.tileState[i][j].groupId();
                if(gps.size() <= gId)
                    gps.resize(gId + 1);

                gps[gId].emplace_back(imgIdx[i][j],
                    std::array<std::ptrdiff_t, 2>({static_cast<std::ptrdiff_t>(i),
                                                   static_cast<std::ptrdiff_t>(j)}));
            }
        });

        return gps;
    }();

    std::vector<Group> groups;
    for (auto& e : gps)
        if (e.size() > 1)
            groups.emplace_back(std::move(e));

    for (auto& g : groups){
        std::array<std::ptrdiff_t, 2> f = std::get<1>(g[0]);
        for (auto& e : g){
            auto& l = std::get<1>(e);
            l[0] -= f[0];
            l[1] -= f[1];
        }
    }

    Remains remain;
    OptionalMap imgMap(pb.div_y());
    DividedImage::foreach(pb, [&](size_t i, size_t j){
        if(param.tileState[i][j].isFree())
            remain.emplace(imgIdx[i][j]);

        if(param.tileState[i][j].isFixed())
            imgMap[i].emplace_back(imgIdx[i][j]);
        else
            imgMap[i].emplace_back(boost::none);
    });

    return std::get<1>(position_bfs(groups.begin(), groups.end(), imgMap, remain, pb, pred));
}


}}