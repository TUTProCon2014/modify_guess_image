
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

//名前空間の指定
using namespace cv;
using namespace std;
using namespace procon;


// 人力
vector<vector<size_t>> modify_guess_image(vector<vector<size_t>> const & before, utils::Problem const & problem);
// マウス操作用のコールバック関数
void Mouse(int event, int x, int y, int flags, void *param);

typedef pair<int, Mat> Pmi;    // 断片テーブル用


//マウス操作のコールバック関数へ渡す引数用の構造体 
struct Paramter
{
    vector<vector<size_t>> before,after;
    Pmi *numbering_table;
    Mat image,base_image;
    size_t width;
    size_t height;
    size_t int_div_x;
    size_t int_div_y;
};


// 画像表示
int main(int argc, char* argv[])
{
    // img = cvLoadImage(FILENAME, CV_LOAD_IMAGE_ANYCOLOR | CV_LOAD_IMAGE_ANYDEPTH);
    auto p_opt = inout::get_problem_from_test_server(1);
    if(p_opt){
        auto& pb = *p_opt;

        vector<vector<size_t>> index;    // インデックス２次元配列

        {
            size_t c = 1;
            for (auto i : utils::iota(pb.div_y())){
                auto cs = utils::iota(c, c + pb.div_x());

                index.emplace_back(cs.begin(), cs.end());
                c += pb.div_x();
            }
        }

        // 試しにシャッフルしてみる
        swap(index[0][0], index[pb.div_y()-1][pb.div_x()-1]);

        //人力修正前
        cout << "修正前" << endl;
        for (auto& ee : index){
            for (auto e: ee)
                    cout << e << ' ';

            cout << endl;
        }
        cout << endl << endl;

        // 人力関数本体
        index = modify_guess_image(index, pb);

        //人力修正後
        cout << "修正後" << endl;
        for (auto& ee : index){
            for (auto e: ee)
                    cout << e << ' ';

            cout << endl;
        }

    }
    return 0;
}


vector<vector<size_t>> modify_guess_image(vector<vector<size_t>> const & before, utils::Problem const & pb)
{
    // 画像読み込み
    Mat src_img = pb.cvMat();

    // エラー処理
    if (src_img.empty()){
        cout << "イメージが空っぽです。関数を終了します" << endl;
        return before;
    }
    if (before.empty()){
        cout << "インデックス２次元配列が空っぽです。関数を終了します" << endl;
        return before;
    }

    cout << "入れ替えたい断片２つをクリックし、ESCキーを押すと断片同士が入れ替わります" << endl;
    cout << "ESCキーを２回連続で押す　もしくは　0キー　を押すと関数を終了します" << endl;

    Pmi numbering_table[257] = {};        /* 
                                        ばらばら状態の画像を左上から右へ向かって1,2,...右下をpb.div_x() * pb.div_y()という感じにナンバリングし、
                                        断片と番号を１つのペアにしたもの
                                        */

    for (int i = 0, num = 1; i < pb.div_y(); i++){
        for (int j = 0; j < pb.div_x(); j++){
            numbering_table[num] =
                Pmi(
                num,
                Mat(src_img,
                    Rect(
                        j*(pb.width() / pb.div_x()),
                        i*(pb.height() / pb.div_y()),
                        (pb.width() / pb.div_x()),
                        (pb.height() / pb.div_y())
                        )
                    )
                );
            num++;
        }
    }


    for(auto& ee :  before)
        for(auto e : ee)
            if(e == 0){
                cout << "中身が存在しない箇所が見つかりました。関数を終了します。" << endl;
            }

    auto after = before;    // 戻り値用


    // 空のMatを用意 width x height 8Bit3Channel(24BitColor)
    Mat base_image(Size(pb.width(), pb.height()), CV_8UC3);

    for(auto i: utils::iota(pb.div_y())){
        for (auto j: utils::iota(pb.div_x())){
            auto dstImg = Mat(base_image,
                Rect(
                    j * (pb.width() / pb.div_x()),
                    i * (pb.height() / pb.div_y()),
                    pb.width() / pb.div_x(),
                    pb.height() / pb.div_y()
                ));
            
            numbering_table[after[i][j]].second
            .copyTo(dstImg);
        }
    }


    std::unique_ptr<Paramter> param(new Paramter);
    param->before = before;
    param->after = after;
    param->numbering_table = &numbering_table[0];
    param->image = src_img;
    param->base_image = base_image;
    param->width = pb.width();
    param->height = pb.height();
    param->int_div_x = pb.div_x();
    param->int_div_y = pb.div_y();


    // ウィンドウ生成
    namedWindow("test", CV_WINDOW_AUTOSIZE);
    imshow("test", base_image)
    cvSetMouseCallback("test", Mouse, param.get());
    waitKey(0);

    after = param->after;
    return after;
}


// マウス操作のコールバック
void Mouse(int event, int x, int y, int flags, void* param_) // コールバック関数
{
    static bool MOUSE_FLAG = false;
    static int tate = -1, yoko = -1,tate2 = -1, yoko2 = -1;

    auto param = static_cast<Paramter*>(param_);
    
    switch (event)
    {
      case CV_EVENT_LBUTTONUP:
        tate2 = (int)(y / (param->height/param->int_div_y));
        yoko2 = (int)(x / (param->width / param->int_div_x));
        if (tate < 0 || yoko < 0){
            tate = tate2;
            yoko = yoko2;
        }
        else if (tate !=  tate2|| yoko != yoko2)
        {
            waitKey(0);
            swap(param->after[tate][yoko], param->after[tate2][yoko2]);
            tate = yoko = -1;


            Mat base_image(Size(param->width, param->height), CV_8UC3);
            Mat Roi[2];


            for (int i = 0; i < param->int_div_y; i++){
                for (int j = 0; j < param->int_div_x; j++){
                    Roi[0] = Mat(param->base_image,
                        Rect(
                        j*(param->width / param->int_div_x),
                        i*(param->height / param->int_div_y),
                        (param->width / param->int_div_x),
                        (param->height / param->int_div_y)
                        )

                        );
                    Roi[1] = param->numbering_table[param->after[i][j]].second;


                    Roi[1].copyTo(Roi[0]);
                    
                }
            }

        }
        
        imshow("test", param->base_image);
        break;

      default:
        break;
    }

    imshow("test", param->base_image);
}
