/*
 Copyright 2016-2017 Amazon.com, Inc. or its affiliates.  All Rights Reserved.

You may not use this file except in compliance with the terms and conditions set forth in
the accompanying LICENSE.TXT file.

THESE MATERIALS ARE PROVIDED ON AN "AS IS" BASIS.  AMAZON SPECIFICALLY DISCLAIMS, WITH
RESPECT TO THESE MATERIALS, ALL WARRANTIES, EXPRESS, IMPLIED, OR STATUTORY, INCLUDING THE
IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND
NON-INFRINGEMENT.
 */




#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <string>
#include <stdint.h>
using namespace std;

//use global variables as all below signals are available to 3rd party lib inside DSP

std::string DBGstring  = "FrameEnergyComputing start";


unsigned int maxFramLen = 256; ////frame length dependent
int max_hang = 24; ////////frame length dependent

int frame = 0;
bool vad = 0;
int wmv = 0;
int64_t engr = 0;
int64_t tmp = 0, sum = 0;
int64_t m_wn21 = 0, m_wn11 = 0, m_wn22 = 0, m_wn12 = 0, m_wn23 = 0, m_wn13 = 0, m_wn24 = 0, m_wn14 = 0;

int64_t voice_engr = 0;
int64_t noise_engr = 0;
//for fix point computation, below float number won't be used
//double coeff_voice_hang = 0.0997f;// = 3/2^5
//double coeff_voice_engr = 0.02f;// = 1229/2^16
//double coeff_noise_engr = 0.0119f;// = 737/2^16

float timestamp = 0;

unsigned int idx = 0;
unsigned int hang = 0;


// the simulator reads in a file line by line for each frame, to simulate what happens in DSP it reads in frame by frame
std::string line = "";
std::string line1 = "";
std::string line2 = "";
std::string line3 = "";
std::string str1 = "Frame";


std::ofstream outfile;
std::ofstream outfile_vad;
std::ofstream outfile_v;
std::ofstream outfile_n;

static void
Compute_Frame_Energy ( )
{
    //cout<<frame<<"  "<<vad<<endl;
    std::istringstream iss2 ( line2 );
    wmv = 0;
    engr = 0;

    for ( idx = 0; idx < maxFramLen; idx ++ ) // FRAMELENGTH16K = maxFramLen samples per frame, 16kHz sampel rate, 15ms per frame
    {
        iss2>>wmv;


        //filtering in fixed point
        //const REAL a1[2] = {-0.5939,    0.2314};
        // w[n] = x[n]- (-0.5939) * m_wn1 - (0.2314) * m_wn2
        sum = wmv + ( ( 19461 * m_wn11 ) >> 15 ) - ( ( 15165 * m_wn21 ) >> 16 );

        //const REAL b1[3] = {0.3209,   -0.6371,    0.3209};
        // y[n] = (0.3209) * sum + (-0.6371) * m_wn1 + (0.3209) * m_wn2
        tmp = ( ( 42061 * sum ) >> 17 ) - ( ( 41753 * m_wn11 ) >> 16 ) + ( ( 42061 * m_wn21 ) >> 17 );

        m_wn21 = m_wn11;
        m_wn11 = sum;

        //------------------------------------------

        //const REAL a2[2] = {-1.4073,    0.7557};
        // w[n] = x[n]- (-1.4073) * m_wn1 - (0.7557) * m_wn2
        sum = tmp + m_wn12 + ( ( 13346 * m_wn12 ) >> 15 ) - ( ( 24763 * m_wn22 ) >> 15 );

        //const REAL b2[3] = {1.0000,   -1.9056,    1.0000};
        // y[n] = (1.0) * sum + (-1.9056) * m_wn1 + (1.0) * m_wn2
        tmp = sum - m_wn12 - ( ( 29675 * m_wn12 ) >> 15 ) + m_wn22;

        m_wn22 = m_wn12;
        m_wn12 = sum;

        //------------------------------------------

        //const REAL a3[2] = {-1.6694,    0.9261};
        // w[n] = x[n]- (-1.6694) * m_wn1 - (0.9261) * m_wn2
        sum = tmp + m_wn13 + ( ( 21935 * m_wn13 ) >> 15 ) - ( ( 30346 * m_wn23 ) >> 15 );

        //const REAL b3[3] = {1.0000,   -1.8409,    1.0000};
        // y[n] = (1.0) * sum + (-1.8409) * m_wn1 + (1.0) * m_wn2
        tmp = sum - m_wn13 - ( ( 27555 * m_wn13 ) >> 15 ) + m_wn23;

        m_wn23 = m_wn13;
        m_wn13 = sum;

        //------------------------------------------

        //const REAL a4[2] = {-1.7503,    0.9830};
        // w[n] = x[n]- (-1.7503) * m_wn1 - (0.9830) * m_wn2
        sum = tmp + m_wn14 + ( ( 24586 * m_wn14 ) >> 15 ) - ( ( 32211 * m_wn24 ) >> 15 );

        //const REAL b4[3] = {1.0000,   -1.8138,    1.0000};
        // y[n] = (1.0) * sum + (-1.8138) * m_wn1 + (1.0) * m_wn2
        wmv = sum - m_wn14 - ( ( 26667 * m_wn14 ) >> 15 ) + m_wn24;

        m_wn24 = m_wn14;
        m_wn14 = sum;



        engr += wmv*wmv;
    }
}

static void
Compute_AccumulativeEngr (  )
{

    //cout << "m0\t\t" << voice_engr << "\t\t" << noise_engr << endl;
    //int64_t engrt = ( 3 * engr ) >> 5;
    //cout << engr << "\t\t" << engrt << endl;
    //engrt = 0;
    //engrt =  ( 3 * engr ) >> 5 + engrt;
    //cout << engr << "\t\t" << engrt << endl;

    if ( hang > 0 )
    {
        hang --;
    }
    if ( hang == 0 )
    {

        //in fixed point
        //voice_engr=voice_engr*(1.0f-coeff_voice_hang);//double coeff_voice_hang = 0.09375f;// = 3/2^5
        //cout << "1-0    " << voice_engr << endl;
        voice_engr = voice_engr - (( 3 * voice_engr ) >> 5);
        //cout << "1-t    " << voice_engr << endl;

    }

    if ( vad )//voice
    {
        hang = max_hang;
        //double coeff_voice_engr = 0.01875f;// = 1229/2^16
        //voice_engr=voice_engr*(1.0f-coeff_voice_engr)+coeff_voice_engr*engr;
        //cout << "2-0    " << voice_engr <<"\t\t"<< engr << endl;
        voice_engr = (( 1331 * engr ) >> 16) + voice_engr - (( 1331 * voice_engr ) >> 16);
        // << "2-t    " << voice_engr <<"\t\t"<< engr << endl;

        outfile << std::fixed;
        outfile << "TIME: " << timestamp * 0.016 << " VAD:" << vad << " EN: " << engr << " Ev: " << voice_engr << " Ea: " << noise_engr << endl;
        timestamp = timestamp + 1.0;

    }
    else //noise
    {
        //double coeff_noise_engr = 0.01125f;// = 737/2^16
        //noise_engr=noise_engr*(1.0f-coeff_noise_engr)+coeff_noise_engr*engr;
        //cout << "3-0    " << noise_engr <<"\t\t"<< engr << endl;
        noise_engr = (( 780 * engr ) >> 16) + noise_engr - (( 780 * noise_engr ) >> 16);
        //cout << "3-t    " << noise_engr <<"\t\t"<< engr << endl;

        if ( hang == 0 )
        {
            //double coeff_voice_engr = 0.01875f;// = 1229/2^16
            //voice_engr=voice_engr*(1.0f-coeff_voice_engr)+coeff_voice_engr*engr;
            //cout << "4-0    " << voice_engr <<"\t\t"<< engr <<endl;
            voice_engr = (( 1331 * engr ) >> 16) + voice_engr - (( 1331 * voice_engr ) >> 16);
            //cout << "4-t    " << voice_engr <<"\t\t"<< engr <<endl;
        }
        outfile << "TIME: " << timestamp * 0.016 << " VAD:" << vad << " EN: " << engr << " Ev: " << voice_engr << " Ea: " << noise_engr << endl;
        timestamp = timestamp + 1.0;
    }
    //cout << "mt\t\t" << voice_engr << "\t\t" << noise_engr << endl;


}
#ifdef X86
#define OUT_FILE "out.txt"
#define OUT_VAD_FILE "out_vad.txt"
#define OUT_VOICE_FILE "out_voice.txt"
#define OUT_NOISE_FILE "out_noise.txt"

#else
#define OUT_FILE "/tmp/web/out.txt"
#define OUT_VAD_FILE "/tmp/web/out_vad.txt"
#define OUT_VOICE_FILE "/tmp/web/out_voice.txt"
#define OUT_NOISE_FILE "/tmp/web/out_noise.txt"
#endif

int
Frame_Energy_Computing(char *fileName,unsigned int *engr_voice,unsigned int *engr_noise)
{
	outfile.open ((char *)OUT_FILE,std::ofstream::out | std::ios::trunc);
	outfile_vad.open ((char *)OUT_VAD_FILE,std::ofstream::out | std::ios::trunc);
	outfile_v.open ((char *)OUT_VOICE_FILE,std::ofstream::out | std::ios::trunc);
	outfile_n.open ((char *)OUT_NOISE_FILE,std::ofstream::out | std::ios::trunc);
	timestamp = 0;
	voice_engr = 0;
	noise_engr = 0;
	sum = 0;
	hang = 0;
	m_wn21 = 0;
	m_wn11 = 0;
	m_wn22 = 0;
	m_wn12 = 0;
	m_wn23 = 0;
	m_wn13 = 0;
	m_wn24 = 0;
	m_wn14 = 0;
    //process input from VAD
    std::istringstream iss ( line );
    std::size_t found;

    cout << DBGstring << endl;

    std::ifstream myfile ( fileName ); //input from VAD

    if ( myfile.is_open ( ) )
    {
        while ( ! myfile.eof ( ) )
        {
            getline ( myfile, line );
            //if (line = something)
            {
                //cout<<line<<std::endl;
                found = line.find ( str1 );
                if ( found != std::string::npos )
                {
                    //read in frame by frame from pseudo input from VAD
                    getline ( myfile, line1 ); //Frame count
                    std::istringstream iss1 ( line1 );
                    iss1>>frame;
                    getline ( myfile, line2 ); //audio data
                    vad = 0;
                    getline ( myfile, line3 ); //VAD result
                    std::istringstream iss3 ( line3 );
                    iss3>>vad;
                    outfile_vad << vad << endl;

                    Compute_Frame_Energy ( );

                    Compute_AccumulativeEngr ( );
                    //cout << "n\t\t" << voice_engr << "\t\t" << noise_engr << endl;
                }
                else
                {
                    break;
                }
                outfile_v << voice_engr << endl;
                outfile_n << noise_engr << endl;
            }
        }
        myfile.close ( );

		if(engr_voice){
			*engr_voice = (unsigned int)voice_engr;
		}
		if(engr_noise){
			*engr_noise = (unsigned int)noise_engr;
		}
    }
    else
        cout << "Unable to open file, note that here we use a fixed input file vadout.txt" << std::endl;


	outfile.close();
	outfile_vad.close();
	outfile_v.close();
	outfile_n.close();

    return 0;


}
