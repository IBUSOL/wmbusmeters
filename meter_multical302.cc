// Copyright (c) 2017 Fredrik Ã–hrstrÃ¶m
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

#include"aes.h"
#include"meters.h"
#include"wmbus.h"
#include"util.h"

#include<memory.h>
#include<stdio.h>
#include<string>
#include<time.h>
#include<vector>

string answer;
string answer2;

using namespace std;

struct MeterMultical302 : public Meter {
    MeterMultical302(WMBus *bus, const char *name, const char *id, const char *key);
    string id();
    string name();

    int totalPower();
    int  totalVolume();
    int currentPower();
 // int  totalOur();
    string typeMessage();	
    string decryptedHex();
 
void onUpdate(function<void(Meter*)> cb);
    int numUpdates();
string datetimeOfUpdateHumanReadable();

    
private:
    void handleTelegram(Telegram*t);
    void processContent(vector<uchar> &d);
    string name_;
    vector<uchar> id_;
    vector<uchar> key_;
    WMBus *bus_;
    vector<function<void(Meter*)>> on_update_;
    int num_updates_;
    bool matches_any_id_;
    time_t datetime_of_update_;

	int totalPower_;
    	int  totalVolume_;
    	int currentPower_;
 //   int  totalOur_;
	string typeMessage_;
	string decryptedHex_;
 	bool use_aes_;
};

MeterMultical302::MeterMultical302(WMBus *bus, const char *name, const char *id, const char *key) :
name_(name), 
bus_(bus), 
num_updates_(0),
matches_any_id_(false), 


totalPower_(0),
totalVolume_(0),
currentPower_(0),
//totalOur_(0),
use_aes_(true)
{
    if (strlen(id) == 0) {
	matches_any_id_ = true;
    } else {
	hex2bin(id, &id_);	    
    }
    if (strlen(key) == 0) {
	use_aes_ = false;
    } else {
	hex2bin(key, &key_);
    }
    bus_->onTelegram(calll(this,handleTelegram,Telegram*));
}

string MeterMultical302::id(){    return bin2hex(id_);}

string MeterMultical302::name(){    return name_;}

void MeterMultical302::onUpdate(function<void(Meter*)> cb){    on_update_.push_back(cb);}

int MeterMultical302::numUpdates(){    return num_updates_;}

string MeterMultical302::datetimeOfUpdateHumanReadable()
{
    char datetime[40];
    memset(datetime, 0, sizeof(datetime));
    strftime(datetime, 20, "%Y-%m-%d %H:%M.%S", localtime(&datetime_of_update_));
    return string(datetime);
}


int MeterMultical302::totalPower(){    return totalPower_;}
int MeterMultical302::totalVolume(){    return totalVolume_;}
int MeterMultical302::currentPower(){    return currentPower_;}
//int MeterMultical302::totalOur(){    return totalOur_;} LATER
string MeterMultical302::typeMessage(){    return typeMessage_;}
string MeterMultical302::decryptedHex(){    return decryptedHex_;}

Meter *createMultical302(WMBus *bus, const char *name, const char *id, const char *key) {
    return new MeterMultical302(bus,name,id,key);
}

void MeterMultical302::handleTelegram(Telegram *t) {

    if (matches_any_id_ || (
	    t->m_field == MANUFACTURER_KAM &&
	    t->a_field_address[3] == id_[3] &&
	    t->a_field_address[2] == id_[2] &&
	    t->a_field_address[1] == id_[1] &&
	    t->a_field_address[0] == id_[0])) {
        verbose("Meter %s receives update with id %02x%02x%02x%02x!\n",
                name_.c_str(),
               t->a_field_address[0], t->a_field_address[1], t->a_field_address[2],
		t->a_field_address[3]);
    } else {
        verbose("Meter %s ignores message with id %02x%02x%02x%02x \n",
                name_.c_str(),
                t->a_field_address[0], t->a_field_address[1], t->a_field_address[2],
		t->a_field_address[3]);
        return;
    }

    // This is part of the wmbus protocol, should be moved to wmbus source files!
    int cc_field = t->payload[0];
    verbose("CC field=%02x ( ", cc_field);
    if (cc_field & CC_B_BIDIRECTIONAL_BIT) verbose("bidir ");
    if (cc_field & CC_RD_RESPONSE_DELAY_BIT) verbose("fast_res ");
    else verbose("slow_res ");
    if (cc_field & CC_S_SYNCH_FRAME_BIT) verbose("synch ");
    if (cc_field & CC_R_RELAYED_BIT) verbose("relayed "); // Relayed by a repeater
    if (cc_field & CC_P_HIGH_PRIO_BIT) verbose("prio ");
    verbose(")\n");
    
    int acc = t->payload[1];
    verbose("ACC field=%02x\n", acc);
    
    uchar sn[4];
    sn[0] = t->payload[2];
    sn[1] = t->payload[3];
    sn[2] = t->payload[4];
    sn[3] = t->payload[5];

    verbose("SN=%02x%02x%02x%02x encrypted=", sn[3], sn[2], sn[1], sn[0]);
    if ((sn[3] & SN_ENC_BITS) == 0) verbose("no\n");
    else if ((sn[3] & SN_ENC_BITS) == 0x40) verbose("yes\n");
    else verbose("? %d\n", sn[3] & SN_ENC_BITS);

    // The content begins with the Payload CRC at offset 6.
    vector<uchar> content;
    content.insert(content.end(), t->payload.begin()+6, t->payload.end());
    size_t remaining = content.size();
    if (remaining > 16) remaining = 16;
    
    uchar iv[16];
    int i=0;
    // M-field
    iv[i++] = t->m_field&255; iv[i++] = t->m_field>>8;
    // A-field
    for (int j=0; j<6; ++j) { iv[i++] = t->a_field[j]; }
    // CC-field
    iv[i++] = cc_field;
    // SN-field
    for (int j=0; j<4; ++j) { iv[i++] = sn[j]; }
    // FN
    iv[i++] = 0; iv[i++] = 0;
    // BC
    iv[i++] = 0;


    if (use_aes_) {
	vector<uchar> ivv(iv, iv+16);
	verbose("Decrypting\n");

	string s = bin2hex(ivv);
	verbose("IV %s\n", s.c_str());

	uchar xordata[16];
	AES_ECB_encrypt(iv, &key_[0], xordata, 16);
	
	uchar decrypt[16];
	xorit(xordata, &content[0], decrypt, remaining);
	
	vector<uchar> dec(decrypt, decrypt+remaining);
	string answer = bin2hex(dec);
	verbose("Decrypted >%s<\n", answer.c_str());
	
	if (content.size() > 26) {

	    fprintf(stderr, "Received too many bytes of content from a Multical302 meter!\n"
		    "Got %zu bytes, expected at most 26.\n", content.size());
	}
	if (content.size() > 16) {
	    // Yay! Lets decrypt a second block. Full frame content is 22 bytes.
	    // So a second block should enough for everyone!
            remaining = content.size()-16;
	    if (remaining > 16) remaining = 16; // Should not happen.
	    
	    incrementIV(iv, sizeof(iv));
	    vector<uchar> ivv2(iv, iv+16);
	    string s2 = bin2hex(ivv2);
	    verbose("IV+1 %s\n", s2.c_str());
	    
	    AES_ECB_encrypt(iv, &key_[0], xordata, 16);
	    
	    xorit(xordata, &content[16], decrypt, remaining);
	    
	    vector<uchar> dec2(decrypt, decrypt+remaining);
	    string answer2 = bin2hex(dec2);
	    verbose("Decrypted second block >%s<\n", answer2.c_str());
		

	    // Append the second decrypted block to the first.
	    dec.insert(dec.end(), dec2.begin(), dec2.end());
	}
	content.clear();
	content.insert(content.end(), dec.begin(), dec.end());
    }

    processContent(content);
 
    datetime_of_update_ = time(NULL);
    num_updates_++;
    for (auto &cb : on_update_) if (cb) cb(this);
}

void MeterMultical302::processContent(vector<uchar> &c) {
    int crc0 = c[0];
    int crc1 = c[1]; 
    int frame_type = c[2];
    verbose("CRC16:      %02x%02x\n", crc1, crc0);
    /*
    uint16_t crc = crc16(&(c[2]), c.size()-2);
    verbose("CRC16 calc: %04x\n", crc);
    */
    if (frame_type == 0x79) {
        verbose("Short frame %d bytes\n", c.size());
	if (c.size() != 17) {
            fprintf(stderr, "Warning! Unexpected length of frame %zu. Expected 17 bytes!\n", c.size());

        }

        int rec1val0 = c[7];
        int rec1val1 = c[8];
        int rec1val2 = c[9];
		
	int rec2val0 = c[13];
        int rec2val1 = c[14];		
	int rec2val2 = c[15];


	  std::string v12 { std::to_string(rec1val1) };



	
	totalPower_ = rec1val2*256*256 + rec1val1*256 + rec1val0;
	verbose("totalPower: %d \n" , totalPower_);
	totalVolume_ = rec2val2*256*256 + rec2val1*256 + rec2val0;
	verbose("totalVolume: %d \n" , totalVolume_);
	typeMessage_ = "SHORT";
	decryptedHex_ = "_" +v12 +"_"; //LATER
	
	
    } else
    if (frame_type == 0x78) {
        verbose("Full frame %d bytes\n", c.size());
	if (c.size() != 26) {
            fprintf(stderr, "Warning! Unexpected length of frame %zu. Expected 26 bytes!\n", c.size());

        }
		
std::string v12 { std::to_string(c[9]) };

	 int rec1val0 = c[24]; 
        int rec1val1 = c[25];

	

	currentPower_ = (rec1val1*256 + rec1val0)*100;
	verbose("currentPower: %d \n" , currentPower_);
	typeMessage_ = "LONG";
        decryptedHex_ = "_" +v12 +"_"; //LATER
  } 
	else {
        fprintf(stderr, "Unknown frame %02x\n", frame_type);
    }
}
