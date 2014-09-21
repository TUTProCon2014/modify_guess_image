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
#include "common.hpp"

namespace procon { namespace modify {

using ImageID = utils::Index2D;

struct Index2DHash
{
    using result_type = std::size_t;

    result_type operator()(ImageID id) const
    {
        std::hash<std::size_t> hs;
        return hs(id[0]) + hs(id[1]);
    }
};

using Group = std::vector<std::tuple<ImageID, std::array<std::ptrdiff_t, 2>>>;
using OptionalMap = std::vector<std::vector<boost::optional<ImageID>>>;


template <typename BinFunc>
double calcAllValue(std::vector<std::vector<ImageID>> const & imgMap, utils::Problem const & pb, BinFunc pred)
{
    double sumV = 0;
    for (auto i : utils::iota(1, pb.div_y()))
        for (auto j : utils::iota(0, pb.div_x())){
            auto idx1 = imgMap[i-1][j],
                idx2 = imgMap[i][j];

            sumV += pred(pb.get_element(idx1[0], idx1[1]), pb.get_element(idx2[0], idx2[1]), utils::Direction::down);
        }

    for (auto i : utils::iota(0, pb.div_y()))
        for (auto j : utils::iota(1, pb.div_x())){
            auto idx1 = imgMap[i][j - 1];
            auto idx2 = imgMap[i][j];
        
            sumV += pred(pb.get_element(idx1[0], idx1[1]), pb.get_element(idx2[0], idx2[1]), utils::Direction::right);
        }

    return sumV;
}


template <typename BinFunc>
std::vector<std::vector<ImageID>>
    fill_remain_tile(
        OptionalMap const & imgMap,
        std::unordered_set<ImageID, Index2DHash> const & remain,
        utils::Problem const & pb,
        BinFunc pred)
{
    OptionalMap before = imgMap;

    PROCON_ENFORCE(before.size() == pb.div_y(), "Contract error: 'before.size() != pb.div_y()'");

    for(auto i: utils::iota(pb.div_y()))
        PROCON_ENFORCE(before[i].size() == pb.div_x(), utils::format("Contract error: 'before[%].size() != pb.div_x()'", i));

    PROCON_ENFORCE(remain.size() < pb.div_x() * pb.div_y(), "Contract error: 'all argument(before)'s elements are null.");

    // beforeでの抜け落ち`where`の周囲について、`which`画像がどの程度マッチするかを返す
    auto around_pred_value = [&](utils::Index2D where, ImageID which){
        const auto i = where[0],
                   j = where[1];

        auto pred_value = [&](utils::Index2D a, utils::Index2D b, utils::Direction dir)
        { /*utils::writefln("%, %, %", a, b, static_cast<size_t>(dir));*/
            return std::abs(pred(pb.get_element(a[0], a[1]), pb.get_element(b[0], b[1]), dir)); };

        double v = 0;
        if(i > 0 && !!before[i-1][j])               v += pred_value(which, *before[i-1][j], utils::Direction::up);
        if(i < pb.div_y() - 1 && !!before[i+1][j])  v += pred_value(which, *before[i+1][j], utils::Direction::down); 
        if(j > 0 && !!before[i][j-1])               v += pred_value(which, *before[i][j-1], utils::Direction::left);
        if(j < pb.div_x() - 1 && !!before[i][j+1])  v += pred_value(which, *before[i][j+1], utils::Direction::right);

        // utils::writefln("%, %, %", where, which, v);
        return v;
    };


    // 次のbeforeでの抜け落ち位置を返す
    auto get_nextTargetIndex = [&](){
        auto targetIndex = boost::optional<utils::Index2D>(boost::none);
        std::size_t maxN = 0;
        for(auto i: utils::iota(pb.div_y()))
            for(auto j: utils::iota(pb.div_x())){
                if(before[i][j])
                    continue;

                std::size_t cnt = 0;
                if(i > 0 && !!before[i-1][j])                   ++cnt;
                if(i < pb.div_y() - 1 && !!before[i+1][j])      ++cnt;
                if(j > 0 && !!before[i][j-1])                   ++cnt;
                if(j < pb.div_x() - 1 && !!before[i][j+1])      ++cnt;

                if(maxN < cnt){
                    maxN = cnt;
                    targetIndex = utils::makeIndex2D(i, j);

                    if(cnt == 4) goto Lreturn;
                }
            }
      Lreturn:
        return targetIndex;
    };


    std::unordered_set<ImageID, Index2DHash> rem = remain;
    while(1){
        auto tgtIdx = get_nextTargetIndex();
        if (!tgtIdx)
            break;

        PROCON_ENFORCE(!before[(*tgtIdx)[0]][(*tgtIdx)[1]], "Error");

        // utils::writeln(*tgtIdx);
        PROCON_ENFORCE(!rem.empty(), "Error, rem.empty() == true");

        double min = std::numeric_limits<double>::infinity();
        boost::optional<utils::Index2D> mostIndex = boost::none;
        for(auto& idx: rem){
            const double v = around_pred_value(*tgtIdx, idx);
            if(v <= min){
                min = v;
                mostIndex = idx;
            }
        }

        // utils::writeln(*mostIndex);
        PROCON_ENFORCE(mostIndex, "Error, mostIndex is null");
        before[(*tgtIdx)[0]][(*tgtIdx)[1]] = *mostIndex;
        rem.erase(*mostIndex);
    }

    std::vector<std::vector<utils::Index2D>> dst; dst.reserve(before.size());
    for(auto i: utils::iota(before.size())){
        dst.emplace_back(before[i].size());
        for(auto j: utils::iota(before[i].size()))
            dst[i][j] = *PROCON_ENFORCE(before[i][j], "Error: all before's elements are null.");
    }

    // utils::writeln(dst);
    return dst;
}


bool is_fit(Group const & group, OptionalMap const & imgMap, std::size_t i, std::size_t j)
{
    if(i >= imgMap.size() || j >= imgMap[i].size())
        return false;

    auto can_put = [&](std::ptrdiff_t i, std::ptrdiff_t j) -> bool {
        if(i >= imgMap.size() || j >= imgMap[i].size() || i < 0 || j < 0) return false;
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
std::tuple<double, std::vector<std::vector<ImageID>>>
    position_bfs(Iter bg, Iter ed,
                 OptionalMap const & imgMap,
                 std::unordered_set<ImageID, Index2DHash>& remain,
                 utils::Problem const & pb,
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

    auto dst = std::make_tuple(std::numeric_limits<double>::infinity(), std::vector<std::vector<utils::Index2D>>());
    utils::DividedImage::foreach(pb, [&](size_t i, size_t j){
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
std::vector<std::vector<utils::Index2D>>
    interactive_guess(Parameter const & param, utils::Problem const & pb, BinFunc pred)
{
    using namespace boost::adaptors;

    auto& imgIdx = param.swpImage.get_index();
    auto gps = [&](){
        std::vector<Group> gps;
        utils::DividedImage::foreach(param.swpImage, [&](size_t i, size_t j){
            if(param.tileState[i][j].isGrouped()){
                const auto gId = param.tileState[i][j].groupId();
                if(gps.size() <= gId)
                    gps.resize(gId + 1);

                gps[gId].emplace_back(imgIdx[i][j], std::array<std::ptrdiff_t, 2>({i, j}));
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

    std::unordered_set<utils::Index2D, Index2DHash> remain;
    OptionalMap imgMap(pb.div_y());
    utils::DividedImage::foreach(pb, [&](size_t i, size_t j){
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