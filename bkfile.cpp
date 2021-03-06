/*! @file dji_with_sony.cpp
 *  @version 3.3
 *  @date Jun 05 2017
 *
 *  @brief
 *  Mobile SDK Communication API usage in a Linux environment.
 *  Shows example usage of the mobile<-->onboard SDK communication API.
 *
 *  @Copyright (c) 2017 DJI
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT,DSTATUS(c); TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include "dji_with_sony.hpp"
#include <pthread.h>
#include <inttypes.h>

#include <cpp_redis/cpp_redis>

#define CH2_NEUTRAL         900
#define CH2_ZOOM_OUT_SPEED1 821
#define CH2_ZOOM_OUT_SPEED2 785
#define CH2_ZOOM_OUT_SPEED3 748
#define CH2_ZOOM_OUT_SPEED4 712
#define CH2_ZOOM_OUT_SPEED5 675
#define CH2_ZOOM_OUT_SPEED6 638
#define CH2_ZOOM_OUT_SPEED7 600
#define CH2_ZOOM_IN_SPEED1  979
#define CH2_ZOOM_IN_SPEED2  1015
#define CH2_ZOOM_IN_SPEED3  1052
#define CH2_ZOOM_IN_SPEED4  1088
#define CH2_ZOOM_IN_SPEED5  1125
#define CH2_ZOOM_IN_SPEED6  1162
#define CH2_ZOOM_IN_SPEED7  1200

#define CH3_NEUTRAL         705
#define CH3_FOCUS           900
#define CH3_SHUTTER_RELEASE 1080

using namespace DJI;
using namespace DJI::OSDK;

// GLOBAL: mobileDataID for keeping track of command from mobile
uint16_t mobileDataID_glob = 0;
// GLOBAL: keepLoopRunning to maintain state of when to stop checking mobile command state
bool keepLoopRunning = true;



void
parseFromMobileCallback(Vehicle* vehicle, RecvContainer recvFrame,
                        UserData userData)
{  
  const char * msg = "Data received: ";
  char buffer[50];
  memset(buffer, 0, sizeof(buffer));
   

  uint16_t c = 0;
  c = *(reinterpret_cast<uint16_t*>(&recvFrame.recvData.raw_ack_array));
   
  sprintf(buffer,"\n %s %c \n", msg, c);
  // DSTATUS(c);
  std::cout << c ;
  std::string data = "CO|S|-1";
  
  if(c==1){
    //connect
    cpp_redis::client client;
    client.connect();
    client.publish("c_connect","1");
    client.sync_commit();    
  
  }
  else if(c==3){
    //zoom in
    cpp_redis::client client;
    client.connect();
    client.publish("c_zoomin","1");
    client.sync_commit();    
  
  }
  else if(c==4){
    //zoom out
    cpp_redis::client client;
    client.connect();
    client.publish("c_zoomout","1");
    client.sync_commit();    
  
  }
  else if(c==5){
    //disconnect
    cpp_redis::client client;
    client.connect();
    client.publish("c_disconnect","1");
    client.sync_commit();    
  
  }
  else{
    if(c>9){
      cpp_redis::client client;
      client.connect();
      int aa=(int)c;
      char str[32];
      sprintf(str,"%d",aa);
      DSTATUS(str);
      client.publish("c_capture",str);
      client.sync_commit(); 
    }
  } 
  
}

bool
setupMSDKParsing(Vehicle* vehicle, LinuxSetup* linuxEnvironment)
{
  
  // First, register the callback for parsing mobile data
  vehicle->moc->setFromMSDKCallback(parseFromMobileCallback, linuxEnvironment);
  DSTATUS("Initial 1. set mobile callback for parsing\n");
  // Then, setup a thread to poll the incoming data, for large functions
  pthread_t threadID = setupSamplePollingThread(vehicle);

  //setup subscribe redis

  cpp_redis::subscriber sub;

  sub.connect();
  sub.subscribe("cam_status", [vehicle](const std::string& chan, const std::string& msg) {
  	std::cout << "Cam status " << chan << ": " << msg << std::endl;
        std::string data = "SA|" + msg +"|" +"-1";   
        sendCustomDataToMobile(vehicle,data);
  });
  sub.subscribe("cam_connected", [vehicle](const std::string& chan, const std::string& msg) {
  	std::cout << "cam connected " << chan << ": " << msg << std::endl;
	std::string data = "CO|" + msg +"|" +"-1";   
        sendCustomDataToMobile(vehicle,data);
  });
  sub.subscribe("capture_status", [vehicle](const std::string& chan, const std::string& msg) {
  	std::cout << "capture " << chan << ": " << msg << std::endl;
	std::string data = "CA|" + msg;   
        sendCustomDataToMobile(vehicle,data);
  });

  sub.commit();


  // User input
  std::cout << "Listening to mobile commands. Press any key to exit.\n";
  char a;
  std::cin >> a;

  // Now that we're exiting, Let's shut off the polling thread
  keepLoopRunning = false;
  void* status;
  pthread_join(threadID, &status);

  return true;
}





void
controlAuthorityMobileCallback(Vehicle* vehiclePtr, RecvContainer recvFrame,
                               UserData userData)
{
  ACK::ErrorCode ack;
  ack.data = OpenProtocolCMD::ErrorCode::CommonACK::NO_RESPONSE_ERROR;

  unsigned char data    = 0x1;
  int           cbIndex = vehiclePtr->callbackIdIndex();

  if (recvFrame.recvInfo.len - OpenProtocol::PackageMin <= sizeof(uint16_t))
  {
    ack.data = recvFrame.recvData.ack;
    ack.info = recvFrame.recvInfo;
  }
  else
  {
    DERROR("ACK is exception, sequence %d\n", recvFrame.recvInfo.seqNumber);
  }

  if (ACK::getError(ack))
  {
    if (ack.data ==
        OpenProtocolCMD::ControlACK::SetControl::OBTAIN_CONTROL_IN_PROGRESS)
    {
      ACK::getErrorCodeMessage(ack, __func__);
      vehiclePtr->obtainCtrlAuthority(controlAuthorityMobileCallback);
    }
    else if (ack.data == OpenProtocolCMD::ControlACK::SetControl::
                           RELEASE_CONTROL_IN_PROGRESS)
    {
      ACK::getErrorCodeMessage(ack, __func__);
      vehiclePtr->releaseCtrlAuthority(controlAuthorityMobileCallback);
    }
    else
    {
      ACK::getErrorCodeMessage(ack, __func__);
    }
  }
  else
  {
    // We have a success case.
    // Send this data to mobile
    AckReturnToMobile mobileAck;
    // Find out which was called: obtain or release
    if (recvFrame.recvInfo.buf[2] == ACK::OBTAIN_CONTROL)
    {
      mobileAck.cmdID = 0x02;
    }
    else if (recvFrame.recvInfo.buf[2] == ACK::RELEASE_CONTROL)
    {
      mobileAck.cmdID = 0x03;
    }
    mobileAck.ack = static_cast<uint16_t>(ack.data);
    vehiclePtr->moc->sendDataToMSDK(reinterpret_cast<uint8_t*>(&mobileAck),
                                    sizeof(mobileAck));
    // Display ACK message
    ACK::getErrorCodeMessage(ack, __func__);
  }
}

void
actionMobileCallback(Vehicle* vehiclePtr, RecvContainer recvFrame,
                     UserData userData)
{
  ACK::ErrorCode ack;

  if (recvFrame.recvInfo.len - OpenProtocol::PackageMin <= sizeof(uint16_t))
  {

    ack.info = recvFrame.recvInfo;

    if (vehiclePtr->isLegacyM600())
      ack.data = recvFrame.recvData.ack;
    else if (vehiclePtr->isM100())
      ack.data = recvFrame.recvData.ack;
    else
      ack.data = recvFrame.recvData.commandACK;

    // Display ACK message
    ACK::getErrorCodeMessage(ack, __func__);

    AckReturnToMobile mobileAck;
    const uint8_t     cmd[] = { recvFrame.recvInfo.cmd_set,
                            recvFrame.recvInfo.cmd_id };

    // startMotor supported in FW version >= 3.3
    // setArm supported only on Matrice 100 && M600 old FW
    if (vehiclePtr->isM100() || vehiclePtr->isLegacyM600())
    {
      if ((memcmp(cmd, OpenProtocolCMD::CMDSet::Control::setArm, sizeof(cmd)) &&
           recvFrame.recvInfo.buf[2] == true))
      {
        mobileAck.cmdID = 0x05;
        mobileAck.ack   = static_cast<uint16_t>(ack.data);
        vehiclePtr->moc->sendDataToMSDK(reinterpret_cast<uint8_t*>(&mobileAck),
                                        sizeof(mobileAck));
      }
      else if ((memcmp(cmd, OpenProtocolCMD::CMDSet::Control::setArm,
                       sizeof(cmd)) &&
                recvFrame.recvInfo.buf[2] == false))
      {
        mobileAck.cmdID = 0x06;
        mobileAck.ack   = static_cast<uint16_t>(ack.data);
        vehiclePtr->moc->sendDataToMSDK(reinterpret_cast<uint8_t*>(&mobileAck),
                                        sizeof(mobileAck));
      }
      else if (recvFrame.recvInfo.buf[2] ==
               Control::FlightCommand::LegacyCMD::takeOff)
      {
        mobileAck.cmdID = 0x07;
        mobileAck.ack   = static_cast<uint16_t>(ack.data);
        vehiclePtr->moc->sendDataToMSDK(reinterpret_cast<uint8_t*>(&mobileAck),
                                        sizeof(mobileAck));
      }
      else if (recvFrame.recvInfo.buf[2] ==
               Control::FlightCommand::LegacyCMD::landing)
      {
        mobileAck.cmdID = 0x08;
        mobileAck.ack   = static_cast<uint16_t>(ack.data);
        vehiclePtr->moc->sendDataToMSDK(reinterpret_cast<uint8_t*>(&mobileAck),
                                        sizeof(mobileAck));
      }
      else if (recvFrame.recvInfo.buf[2] ==
               Control::FlightCommand::LegacyCMD::goHome)
      {
        mobileAck.cmdID = 0x09;
        mobileAck.ack   = static_cast<uint16_t>(ack.data);
        vehiclePtr->moc->sendDataToMSDK(reinterpret_cast<uint8_t*>(&mobileAck),
                                        sizeof(mobileAck));
      }
    }
    else // Newer firmware
    {
      if (recvFrame.recvInfo.buf[2] == Control::FlightCommand::startMotor)
      {
        mobileAck.cmdID = 0x05;
        mobileAck.ack   = static_cast<uint16_t>(ack.data);
        vehiclePtr->moc->sendDataToMSDK(reinterpret_cast<uint8_t*>(&mobileAck),
                                        sizeof(mobileAck));
      }
      else if (recvFrame.recvInfo.buf[2] == Control::FlightCommand::stopMotor)
      {
        mobileAck.cmdID = 0x06;
        mobileAck.ack   = static_cast<uint16_t>(ack.data);
        vehiclePtr->moc->sendDataToMSDK(reinterpret_cast<uint8_t*>(&mobileAck),
                                        sizeof(mobileAck));
      }
      else if (recvFrame.recvInfo.buf[2] == Control::FlightCommand::takeOff)
      {
        mobileAck.cmdID = 0x07;
        mobileAck.ack   = static_cast<uint16_t>(ack.data);
        vehiclePtr->moc->sendDataToMSDK(reinterpret_cast<uint8_t*>(&mobileAck),
                                        sizeof(mobileAck));
      }
      else if (recvFrame.recvInfo.buf[2] == Control::FlightCommand::landing)
      {
        mobileAck.cmdID = 0x08;
        mobileAck.ack   = static_cast<uint16_t>(ack.data);
        vehiclePtr->moc->sendDataToMSDK(reinterpret_cast<uint8_t*>(&mobileAck),
                                        sizeof(mobileAck));
      }
      else if (recvFrame.recvInfo.buf[2] == Control::FlightCommand::goHome)
      {
        mobileAck.cmdID = 0x09;
        mobileAck.ack   = static_cast<uint16_t>(ack.data);
        vehiclePtr->moc->sendDataToMSDK(reinterpret_cast<uint8_t*>(&mobileAck),
                                        sizeof(mobileAck));
      }
    }
  }
  else
  {
    DERROR("ACK is exception, sequence %d\n", recvFrame.recvInfo.seqNumber);
  }
}

void
sendCustomDataToMobile(DJI::OSDK::Vehicle* vehiclePtr, std::string data)
{
  char        tempRawString[38] = { 0 };
  memcpy(&tempRawString, data.c_str(), data.length());
  // Now pack this up into a mobile packet
  VersionMobilePacket versionPack(0x01, &tempRawString[0]);
  std::cout << "data:" << &tempRawString[0];
  // And send it
  vehiclePtr->moc->sendDataToMSDK(reinterpret_cast<uint8_t*>(&versionPack),
                                  sizeof(versionPack));
}

void
sendDroneVersionFromCache(DJI::OSDK::Vehicle* vehiclePtr)
{
  // We will reformat the cached drone version and send it back.
  std::string hwVersionString(vehiclePtr->getHwVersion());
  std::string fwVersionString   = std::to_string(vehiclePtr->getFwVersion());
  std::string versionString     = hwVersionString + ' ' + fwVersionString;
  char        tempRawString[38] = { 0 };
  memcpy(&tempRawString, versionString.c_str(), versionString.length());
  // Now pack this up into a mobile packet
  VersionMobilePacket versionPack(0x01, &tempRawString[0]);

  // And send it
  vehiclePtr->moc->sendDataToMSDK(reinterpret_cast<uint8_t*>(&versionPack),
                                  sizeof(versionPack));
}

VersionMobilePacket::VersionMobilePacket(uint16_t cmdID, char* versionPack)
  : version{ 0 }
{
  this->cmdID = cmdID;
  memcpy(this->version, versionPack, sizeof(version) / sizeof(version[0]));
}

void
activateMobileCallback(Vehicle* vehiclePtr, RecvContainer recvFrame,
                       UserData userData)
{

  ACK::ErrorCode ack;

  // First, let's go through the same steps the default Vehicle callback does
  // for activation
  // since activation status is necessary for  the rest of the system to
  // function properly.

  uint16_t ack_data;
  if (recvFrame.recvInfo.len - OpenProtocol::PackageMin <= 2)
  {
    ack_data = recvFrame.recvData.ack;

    ack.data = ack_data;
    ack.info = recvFrame.recvInfo;

    if (ACK::getError(ack) &&
        ack_data ==
          OpenProtocolCMD::ErrorCode::ActivationACK::OSDK_VERSION_ERROR)
    {
      DERROR("SDK version did not match\n");
      vehiclePtr->getDroneVersion();
    }

    //! Let user know about other errors if any
    ACK::getErrorCodeMessage(ack, __func__);
  }
  else
  {
    DERROR("ACK is exception, sequence %d\n", recvFrame.recvInfo.seqNumber);
  }

  if (ack_data == OpenProtocolCMD::ErrorCode::ActivationACK::SUCCESS &&
      vehiclePtr->getAccountData().encKey)
  {
    vehiclePtr->protocolLayer->setKey(vehiclePtr->getAccountData().encKey);
  }

  // Now, we have to send the ACK back to mobile
  AckReturnToMobile mobileAck;

  mobileAck.cmdID = 0x04;
  mobileAck.ack   = (uint16_t)ack.data;

  vehiclePtr->moc->sendDataToMSDK(reinterpret_cast<uint8_t*>(&mobileAck),
                                  sizeof(mobileAck));
}

void
sendAckToMobile(DJI::OSDK::Vehicle* vehiclePtr, uint16_t cmdID, uint16_t ack)
{
  // Generate a local ACK to send the ACK back to mobile
  AckReturnToMobile mobileAck;
  mobileAck.cmdID = cmdID;
  mobileAck.ack   = ack;
   std::cout << "sending mobile";
  vehiclePtr->moc->sendDataToMSDK(reinterpret_cast<uint8_t*>(&mobileAck),
                                  sizeof(mobileAck));
  std::cout << "send back to mobile";
}

bool
runPositionControlSample(Vehicle* vehicle)
{
  bool positionControlError = false;
  positionControlError      = monitoredTakeoff(vehicle);
  positionControlError &= moveByPositionOffset(vehicle, 0, 6, 6, 30);
  positionControlError &= moveByPositionOffset(vehicle, 6, 0, -3, -30);
  positionControlError &= moveByPositionOffset(vehicle, -6, -6, 0, 0);
  positionControlError &= monitoredLanding(vehicle);

  return (!positionControlError); // We want to return success status, not error
                                  // status
}

pthread_t setupSamplePollingThread(Vehicle* vehicle)
{
  int         ret = -1;
  std::string infoStr;

  pthread_t      threadID;
  pthread_attr_t attr;

  /* Initialize and set thread detached attribute */
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

  ret     = pthread_create(&threadID, NULL, mobileSamplePoll, (void*)vehicle);
  infoStr = "mobilePoll";

  if (0 != ret)
  {
    DERROR("fail to create thread for %s!\n", infoStr.c_str());
  }

  ret = pthread_setname_np(threadID, infoStr.c_str());
  if (0 != ret)
  {
    DERROR("fail to set thread name for %s!\n", infoStr.c_str());
  }
  return threadID;

}

void *
mobileSamplePoll(void* vehiclePtr)
{
  Vehicle* vehicle = (Vehicle *)vehiclePtr;

  // Initialize variables so as to not cross case statements
  bool coreMissionStatus = false;
  int wayptPolygonSides, hotptInitRadius;
  int responseTimeout = 1;

  // Run polling loop until we're told to stop
  while (keepLoopRunning)
  {
    // Check global variable to find out if we need to execute an action
    switch (mobileDataID_glob)
    {
      case 0x3E:
        coreMissionStatus = runPositionControlSample(vehicle);
        sendAckToMobile(vehicle, 0x3E, coreMissionStatus);
        mobileDataID_glob = 0;
        break;
      case 0x40:
        coreMissionStatus = gimbalCameraControl(vehicle);
        sendAckToMobile(vehicle, 0x40, coreMissionStatus);
        mobileDataID_glob = 0;
        break;
      case 0x41:
        wayptPolygonSides = 6;
        coreMissionStatus =
            runWaypointMission(vehicle, wayptPolygonSides, responseTimeout);
        sendAckToMobile(vehicle, 0x41, coreMissionStatus);
        mobileDataID_glob = 0;
        break;
      case 0x42:
        hotptInitRadius = 10;
        coreMissionStatus =
            runHotpointMission(vehicle, hotptInitRadius, responseTimeout);
        sendAckToMobile(vehicle, 0x42, coreMissionStatus);
        mobileDataID_glob = 0;
        break;
      default:
        break;
    }
    //usleep(500000);
    usleep(5000);
  }

}
