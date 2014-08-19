//インクルードファイル指定
#include <opencv2/opencv.hpp>
//静的リンクライブラリの指定
#include <opencv2/opencv_lib.hpp>
#include <opencv/highgui.h>

#include <windows.h>

//名前空間の指定
using namespace cv;
using namespace std;

// 人力
vector<vector<size_t>> modify_guess_image(vector<vector<size_t>> before, IplImage* image, int width, int height, int int_div_x, int int_div_y/*, Problem const & problem*/);
// マウス操作用のコールバック関数
void Mouse(int event, int x, int y, int flags, void *imgA);

typedef pair<int, Mat> Pmi;	// 断片テーブル用


//マウス操作のコールバック関数へ渡す引数用の構造体 
struct Paramter
{
	vector<vector<size_t>> before,after;
	Pmi *numbering_table;
	Mat image,base_image;
	int width;
	int height;
	int int_div_x;
	int int_div_y;

}paramter;

// イメージ
IplImage *img = cvLoadImage("画像.png", CV_LOAD_IMAGE_ANYCOLOR | CV_LOAD_IMAGE_ANYDEPTH);	// ばらばら画像
int int_div_x = 5, int_div_y = 5;	//分割数


// 画像表示
int main(int argc, char* argv[])
{

	vector<size_t> v;	//インデックス２次元配列生成用
	vector<vector<size_t>> index;	// インデックス２次元配列

	
	for (int i = 0, c = 1; i < int_div_y; i++){
		v.clear();
		for (int j = 0; j < int_div_x; j++,c++){
			{
				v.push_back(c);
			}
		}
		index.push_back(v);
	}
	// 試しにシャッフルしてみる
	swap(index[0][0], index[int_div_y-1][int_div_x-1]);

	//人力修正前
	cout << "修正前" << endl;
	for (int i = 0, c = 1; i < int_div_y; i++){
		for (int j = 0; j < int_div_x; j++, c++){
			{
				cout << index[i][j] << ' ';
			}
		}
		cout << endl;
	}
	cout << endl << endl;


	// 人力関数本体
	index = modify_guess_image(index, img, img->width, img->height, int_div_x, int_div_y);




	//人力修正後
	cout << "修正後" << endl;
	for (int i = 0, c = 1; i < int_div_y; i++){
		for (int j = 0; j < int_div_x; j++, c++){
			{
				cout << index[i][j] << ' ';
			}
		}
		cout << endl;
	}
	waitKey(0);
	return 0;
}


vector<vector<size_t>> modify_guess_image(vector<vector<size_t>> before, IplImage* image, int width, int height, int int_div_x, int int_div_y/*, Problem const & problem*/)
{
	// 画像読み込み
	Mat src_img(image);

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

	
	Pmi numbering_table[257] = {};		/* 
										ばらばら状態の画像を左上から右へ向かって1,2,...右下をint_div_x * int_div_yという感じにナンバリングし、
										断片と番号を１つのペアにしたもの
										*/

	for (int i = 0, num = 1; i < int_div_y; i++){
		for (int j = 0; j < int_div_x; j++){
			numbering_table[num] =
				Pmi(
				num,
				Mat(src_img,
					Rect(
						j*(width / int_div_x),
						i*(height / int_div_y),
						(width / int_div_x),
						(height / int_div_y)
						)
					)
				);
			num++;
		}
	}
	

	vector<vector<size_t>> after;	// 戻り値用
	for (int i = 0; i < before.size(); i++){
		for (int j = 0; j < before[i].size(); j++){
			{
				if (before[i][j] == NULL){
					cout << "中身が存在しない箇所が見つかりました。関数を終了します。" << endl;
				}
			}
		}
		after.push_back(before[i]);
	}


	
	// 空のMatを用意 width x height 8Bit3Channel(24BitColor)
	Mat base_image(Size(width, height), CV_8UC3);
	Mat Roi[2];


	for (int i = 0; i < int_div_y; i++){
		for (int j = 0; j < int_div_x; j++){
			Roi[0] = Mat(base_image,
				Rect(
				j*(width / int_div_x),
				i*(height / int_div_y),
				(width / int_div_x),
				(height / int_div_y)
				)

				);
			Roi[1] = numbering_table[after[i][j]].second;

				
			Roi[1].copyTo(Roi[0]);

		}
	}
	
	/*コールバック関数の引き渡しがうまく出来なかったのでグローバル関数でパラメータを渡しています*/
	paramter.before = before;
	paramter.after = after;
	paramter.numbering_table = &numbering_table[0];
	paramter.image = src_img;
	paramter.base_image = base_image;
	paramter.width = width;
	paramter.height = height;
	paramter.int_div_x = int_div_x;
	paramter.int_div_y = int_div_y;
	

	//imshow("test", base_image);
	// ウィンドウ生成

	namedWindow("test", CV_WINDOW_AUTOSIZE);
	cvSetMouseCallback("test", Mouse, 0);
	waitKey(0);

	after = paramter.after;
	return after;
}

// マウス操作のコールバック
void Mouse(int event, int x, int y, int flags, void* param) // コールバック関数
{
	static bool MOUSE_FLAG = false;
	static int tate = -1, yoko = -1,tate2 = -1, yoko2 = -1;
	

	
	switch (event)
	{

	case CV_EVENT_LBUTTONUP:
		tate2 = (int)(y / (paramter.height/paramter.int_div_y));
		yoko2 = (int)(x / (paramter.width / paramter.int_div_x));
		if (tate < 0 || yoko < 0){
			tate = tate2;
			yoko = yoko2;
		}
		else if (tate !=  tate2|| yoko != yoko2)
		{
		//	cout << tate << ' ' << yoko << ' ';
			waitKey(0);
			swap(paramter.after[tate][yoko], paramter.after[tate2][yoko2]);
			tate = yoko = -1;


			Mat base_image(Size(paramter.width, paramter.height), CV_8UC3);
			Mat Roi[2];


			for (int i = 0; i < paramter.int_div_y; i++){
				for (int j = 0; j < paramter.int_div_x; j++){
					Roi[0] = Mat(paramter.base_image,
						Rect(
						j*(paramter.width / paramter.int_div_x),
						i*(paramter.height / paramter.int_div_y),
						(paramter.width / paramter.int_div_x),
						(paramter.height / paramter.int_div_y)
						)

						);
					Roi[1] = paramter.numbering_table[paramter.after[i][j]].second;


					Roi[1].copyTo(Roi[0]);
					
				}
			}

		}
		
		imshow("test", paramter.base_image);
		break;

	}

	imshow("test", paramter.base_image);
}
