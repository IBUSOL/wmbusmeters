// Copyright (c) 2017 Fredrik Öhrström
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include"printer.h"

using namespace std;

Printer::Printer(bool robot, bool meterfiles)
{
    robot_ = robot;
    meterfiles_ = meterfiles;
}

void Printer::print(Meter *meter) 
{
    FILE *output = stdout;
    
    if (meterfiles_) {
	char filename[128];
	memset(filename, 0, sizeof(filename));
	snprintf(filename, 127, "/home/pi/files/%s", meter->name().c_str());
	output = fopen(filename, "ab");
    }

    if (robot_) printMeterJSON(output, meter);
    else printMeterHumanReadable(output, meter);
    
    if (output != stdout) {
	fclose(output);
    }        
}

void Printer::printMeterHumanReadable(FILE *output, Meter *meter)
{
    fprintf(output , "%s;%s;%d;%d;%d;%s;%s;%s\n",
	    meter->name().c_str(),
	    meter->id().c_str(),
		meter->totalPower(),
		meter->totalVolume(),
		meter->currentPower(),
        meter->datetimeOfUpdateHumanReadable().c_str(),
 		meter->typeMessage().c_str(),
 		meter->decryptedHex().c_str()
		);	
}

#define Q(x,y) "\""#x"\":"#y","
#define QS(x,y) "\""#x"\":\""#y"\","
#define QSE(x,y) "\""#x"\":\""#y"\""

void Printer::printMeterJSON(FILE *output, Meter *meter)
{
    fprintf(output, "{"
	    QS(name,%s)
	    QS(id,%s)
	    Q(total_power_kwh,%d)
	   	Q(totalVolume_m3,%d)
		Q(currentPower_W,%d)
		QS(Timestamp,%s)
	    "}\n",
	    meter->name().c_str(),
	    meter->id().c_str(),
	    meter->totalPower(),
	    meter->totalVolume(),
        meter->currentPower(),
        meter->datetimeOfUpdateHumanReadable().c_str());
}