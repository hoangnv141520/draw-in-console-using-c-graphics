#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <winbgim.h>
#include <math.h>
using namespace std;
typedef struct {
    string type;
    int x1, y1, x2, y2;
    int ctrlx1, ctrly1, ctrlx2, ctrly2;
} Data;
void readDataFromFile(const char *filename, vector<Data> &dataArray) {
    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "Unable to open file" << endl;
        exit(EXIT_FAILURE);
    }
    Data data;
    string buffer;
    int size;
    file >> size;
    getline(file, buffer);

    for (int i = 0; i < size; ++i) {
        getline(file, buffer);
        istringstream bufferStream(buffer);
        bufferStream >> data.type
                     >> data.x1 >> data.y1
                     >> data.ctrlx1 >> data.ctrly1
                     >> data.ctrlx2 >> data.ctrly2
                     >> data.x2 >> data.y2;
        dataArray.push_back(data);
    }
    file.close();
}
void cubicBezier(int x1, int y1, int ctrlx1, int ctrly1, int ctrlx2, int ctrly2, int x2, int y2, float step, int color, int up){
	for (float t = 0; t <= 1; t+=step){
		int x = pow(1-t,3)*x1 + 3*pow(1-t,2)*t*(ctrlx1) + 3*(1-t)*pow(t,2)*ctrlx2 + pow(t,3)*x2;
		int y = pow(1-t,3)*y1 + 3*pow(1-t,2)*t*(ctrly1) + 3*(1-t)*pow(t,2)*ctrly2 + pow(t,3)*y2;
		putpixel(x, y+up, color);
	}
}
void quadraticBezier(int x1, int y1, int ctrlx1, int ctrly1, int x2, int y2, float step, int color, int up){
	for (float t = 0; t <= 1; t+=step){
		int x = (1-t)*((1-t)*x1 + t*ctrlx1) + t*((1-t)*ctrlx1+t*x2);
		int y = (1-t)*((1-t)*y1 + t*ctrly1) + t*((1-t)*ctrly1+t*y2);
		putpixel(x, y+up, color);
	}
}
int main() {
	int numberOfData = 1000;
	initwindow(500, 500);
	float step = 0.01;
	int color = 3;
	int up = 0;
//	setbkcolor(4);
    setwindowtitle("Dance every where");
	for(int i = 1; i <= numberOfData; i++){	
		ostringstream oss;
        oss << "D:\\img\\bruh.txt";
        string path = oss.str();
        vector<Data> dataArray;
        readDataFromFile(path.c_str(), dataArray);
    	for (int i = 0; i < dataArray.size(); i++) {
        	int x1 = dataArray[i].x1/10, x2 = dataArray[i].x2/10, 
			y1 = 500-dataArray[i].y1/10, y2 = 500-dataArray[i].y2/10;
			int ctrlx1= dataArray[i].ctrlx1/10, ctrlx2 = dataArray[i].ctrlx2/10, 
			ctrly1 = 500-dataArray[i].ctrly1/10, ctrly2 = 500-dataArray[i].ctrly2/10;
			if(dataArray[i].type == "CubicBezier"){
				cubicBezier(x1, y1, ctrlx1, ctrly1, ctrlx2, ctrly2, x2, y2, step, color, up);
			}else if(dataArray[i].type == "QuadraticBezier"){
				quadraticBezier(x1, y1, ctrlx1, ctrly1, x2, y2, step, color, up);
			}else if(dataArray[i].type == "Line"){
				setcolor(color);
				line(x1, y1, x2, y2);
			}
    	}
    	
    	delay(5000000);
    	cleardevice();
	}
    delay(0);
    closegraph();
    return 0;
}

