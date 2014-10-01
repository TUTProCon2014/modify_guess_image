
//インクルードファイル指定
#include <opencv2/opencv.hpp>
//静的リンクライブラリの指定
// #include <opencv2/opencv_lib.hpp>
#include <opencv/highgui.h>
#include <memory>

#include "../inout/include/inout.hpp"
#include "../utils/include/types.hpp"
#include "../utils/include/image.hpp"
#include "../utils/include/range.hpp"
#include "interactive_guess.hpp"
#include "modify_guess_image.hpp"
#include "../guess_img/include/guess.hpp"
#include "../guess_img/include/blocked_guess.hpp"
#include "../utils/include/backtrace.hpp"

using namespace procon;


// 画像表示
int main(int argc, char* argv[])
{
    utils::write("問題番号 --- ");
    size_t pId = 1;
    std::cin >> pId;

    // img = cvLoadImage(FILENAME, CV_LOAD_IMAGE_ANYCOLOR | CV_LOAD_IMAGE_ANYDEPTH);
    auto p_opt = utils::Problem::get(utils::format("img%.ppm", pId));

    if(!p_opt)
        p_opt = inout::get_problem_from_test_server(pId);

    if(p_opt){
        auto& pb = *p_opt;
        
        auto pred = [](utils::Image const & img1, utils::Image const & img2, utils::Direction dir)
        { return guess::diff_connection(img1, img2, dir); };

        auto idxs = blocked_guess::guess(pb, pred);
        
        try{
            auto after = modify::modify_guess_image(idxs, pb, [](std::vector<std::vector<utils::ImageID>> imgMap){
                utils::writeln("send");
            });
        }
        catch (std::exception& ex){
            utils::writeln(ex);
            throw ex;
        }
    }
    return 0;
}
