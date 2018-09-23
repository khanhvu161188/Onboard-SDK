//------------------------------------------------------------------------------
// File:        example3.cpp
//
// Description: Example to read or write metadata from files
//
// Syntax:      example3 [OPTIONS] FILE [FILE...]
//
// Options:     Any single-argument options supported by the exiftool
//              application, including options of the form -TAG=VALUE to write
//              information to the specified file(s).
//
// License:     Copyright 2013-2018, Phil Harvey (phil at owl.phy.queensu.ca)
//
//              This is software, in whole or part, is free for use in
//              non-commercial applications, provided that this copyright notice
//              is retained.  A licensing fee may be required for use in a
//              commercial application.
//
// Created:     2013-11-23 - Phil Harvey
//------------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ExifTool.h"
#include <sstream>
#include <iostream>
using namespace std;

int main(int argc, char **argv)
{
    double FlightPitchDegree;
    double FlightRollDegree;
    double FlightYawDegree;

    double GimbalPitchDegree;
    double GimbalRollDegree;
    double GimbalYawDegree;

    ExifTool *et = new ExifTool();
//700 497
    for (int i=494; i<700; ++i) { 
        char clearMakerNotesString[50];
        sprintf(clearMakerNotesString, "-overwrite_original\n-MakerNotes:all=\nDSC-%d.jpg", i);

        char cloneDJIMakerNotesString[100];
        sprintf(cloneDJIMakerNotesString, "-tagsfromfile\nsample.jpg\n-overwrite_original\n-makernotes\n-make\n-model\nDSC-%d.jpg", i);
          
	if (!et->IsRunning()) {
            printf("ExifTool process is not running\n");
            break;
        }
        char getInfoString[32];
        sprintf(getInfoString, "DSC-%d.jpg", i);



        // read metadata from the image
	TagInfo *info = et->ImageInfo(getInfoString);
	if (info) {
	// print returned information
	for (TagInfo *j=info; j; j=j->next) {
	    if(strcmp(j->name,"FlightPitchDegree")==0)
	    {
                
                FlightPitchDegree=  atof(j->value) ;
		if(FlightPitchDegree>360){
                        printf("file: DSC-%d.jpg \n",i);
			cout << j->name << " = " << j->value << endl;
			FlightPitchDegree = FlightPitchDegree * 0.1;
		}
	        
            }

	    else  if(strcmp(j->name,"FlightRollDegree")==0)
	    {
                FlightRollDegree=  atof(j->value) ;
		if(FlightRollDegree>360){
                        printf("file: DSC-%d.jpg \n",i);
			cout << j->name << " = " << j->value << endl;
			FlightRollDegree = FlightRollDegree * 0.1;
		}
            }	
 	    else if(strcmp(j->name,"FlightYawDegree")==0)
	    {
              
  		FlightYawDegree=  atof(j->value) ;
		if(FlightYawDegree>360){
                        printf("file: DSC-%d.jpg \n",i);
			cout << j->name << " = " << j->value << endl;
                        FlightYawDegree = FlightYawDegree * 0.1;
		}
            }	
	    else  if(strcmp(j->name,"GimbalPitchDegree")==0)
	    {
                
  		GimbalPitchDegree=  atof(j->value) ;
		if(GimbalPitchDegree>360){
                        printf("file: DSC-%d.jpg \n",i);
			cout << j->name << " = " << j->value << endl; 
                        GimbalPitchDegree = FlightPitchDegree;
		}
            }	
            else  if(strcmp(j->name,"GimbalRollDegree")==0)
	    {
                GimbalRollDegree=  atof(j->value) ;
		if(GimbalRollDegree>360){
                        printf("file: DSC-%d.jpg \n",i);
			cout << j->name << " = " << j->value << endl;
                        GimbalRollDegree = FlightRollDegree;
		}
            }	
 	    else  if(strcmp(j->name,"GimbalYawDegree")==0)
	    {
                
  		GimbalYawDegree=  atof(j->value) ;
	        if(GimbalYawDegree>360){
                        printf("file: DSC-%d.jpg \n",i);
			cout << j->name << " = " << j->value << " flight degree " << FlightYawDegree << endl;
                        GimbalYawDegree = FlightYawDegree;
		}
            }		
	    else{
	    }	
	   
	}
	// we are responsible for deleting the information when done
	    delete info;
	} 
	else if (et->LastComplete() <= 0) {
	cerr << "Error executing exiftool!" << endl;
	}


        int cmdNum = et->Command(clearMakerNotesString);
        et->Complete(3); 
	       
        // print the stdout message returned by exiftool
        char *out = et->GetOutput();
        if (out) printf("1.%s", out);
        char *err = et->GetError();
        if (err) printf("1.>>>>>>>>\n%s", err);

        cmdNum = et->Command(cloneDJIMakerNotesString);
        et->Complete(3); 
        out = et->GetOutput();
        if (out) printf("2. %s", out);
        err = et->GetError();
        if (err) printf("2. >>>>>>>>\n%s", err);

        et->SetNewValue("Yaw", to_string(FlightYawDegree).c_str());
        et->SetNewValue("Pitch", to_string(FlightPitchDegree).c_str());
        et->SetNewValue("Roll", to_string(FlightRollDegree).c_str());

        et->SetNewValue("CameraYaw", to_string(GimbalYawDegree).c_str());
        et->SetNewValue("CameraPitch", to_string(GimbalPitchDegree).c_str());
        et->SetNewValue("CameraRoll", to_string(GimbalRollDegree).c_str());
        cmdNum = et->WriteInfo(getInfoString, "-overwrite_original");
      
        int result = et->Complete(5); 

	    if (result > 0) {
		// checking either the number of updated images or the number of update
		// errors should be sufficient since we are only updating one file,
		// but check both for completeness
		int n_updated = et->GetSummary(SUMMARY_IMAGE_FILES_UPDATED);
		int n_errors = et->GetSummary(SUMMARY_FILE_UPDATE_ERRORS);
		if (n_updated == 1 && n_errors <= 0) {
		    cout << "Success!" << endl;
		} else {
		    cerr << "The exiftool application was not successful." << endl;
		}
		// print any exiftool messages
		char *out1 = et->GetOutput();
		if (out1) cout << out1;
		char *err1 = et->GetError();
		if (err1) cerr << err1;
	    } else {
		cerr << "Error executing exiftool command!" << endl;
	    }
        char *out3 = et->GetOutput();
        if (out3) printf("3.%s", out3);
        char *err3 = et->GetError();
        if (err3) printf("3.>>>>>>>>\n%s", err3);
    }   

    delete et;  // must delete our ExifTool object when done
    return 0;
}

std::string to_string(double x)
{
  std::ostringstream ss;
  ss << x;
  return ss.str();
}
// end
