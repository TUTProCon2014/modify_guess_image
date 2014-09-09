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

template <typename BinFunc>
std::vector<std::vector<utils::Index2D>>
    interactive_guess(
        std::vector<std::vector<boost::optional<utils::Index2D>>> before,
        utils::Problem const & pb,
        BinFunc pred)
{
    PROCON_ENFORCE(before.size() == pb.div_y(), "Contract error: 'before.size() != pb.div_y()'");

    for(auto i: utils::iota(pb.div_y()))
        PROCON_ENFORCE(before[i].size() == pb.div_x(), utils::format("Contract error: 'before[%].size() != pb.div_x()'", i));

    std::set<utils::Index2D> rem;
    for(auto i: utils::iota(pb.div_y()))
        for(auto j: utils::iota(pb.div_x()))
            rem.emplace(utils::makeIndex2D(i, j));

    bool allNullTest = false;
    for(auto i: utils::iota(pb.div_y()))
        for(auto j: utils::iota(pb.div_x()))
            if(before[i][j]){
                rem.erase(*before[i][j]);
                allNullTest = true;
            }

    PROCON_ENFORCE(allNullTest, "Contract error: 'all argument(before)'s elements are null.");


    // beforeでの抜け落ち`where`の周囲について、`which`画像がどの程度マッチするかを返す
    auto around_pred_value = [&](utils::Index2D where, utils::Index2D which){
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


}}