// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdlib.h>

#include <stdio.h>
#include <stdint.h>

/* This sample uses the _LL APIs of iothub_client for example purposes.
That does not mean that HTTP only works with the _LL APIs.
Simply changing the using the convenience layer (functions not having _LL)
and removing calls to _DoWork will yield the same results. */

#ifdef ARDUINO
#include "AzureIoTHub.h"
#else
#include "azure_c_shared_utility/threadapi.h"
#include "azure_c_shared_utility/platform.h"
#include "serializer.h"
#include "iothub_client_ll.h"
#include "iothubtransporthttp.h"
#endif

#ifdef MBED_BUILD_TIMESTAMP
#include "certs.h"
#endif // MBED_BUILD_TIMESTAMP

/*String containing Hostname, Device Id & Device Key in the format:             */
/*  "HostName=<host_name>;DeviceId=<device_id>;SharedAccessKey=<device_key>"    */
//static const char* connectionString = "HostName=PdM-Dashboard.azure-devices.net;DeviceId=PdM-Jump1;SharedAccessKey=7s3ZviOSxja+zkOxBMpim4spGIq2JBfpSmxWVHjVXG4=";
static const char* connectionString = "HostName=DDS-IOT-Sample01-MainHub.azure-devices.net;DeviceId=CC3220_board;SharedAccessKey=DAvvKwiBCofQHDLYrf7k3+FSJJa4VIslLi0Y4sHkXsc=";
//static const char* connectionString = "HostName=DDS-IOT-Sample01-MainHub.azure-devices.net;SharedAccessKeyName=iothubowner;SharedAccessKey=PNi1qvkJV+zAqW/r2q9rXJAL87GHU9f8QepAzmNoh+E=";

// Define the Model

BEGIN_NAMESPACE(MotorMonitor);

DECLARE_MODEL(PdMMotorMonitor,
WITH_DATA(ascii_char_ptr, Timestamp),
WITH_DATA(ascii_char_ptr, TargetSite),
WITH_DATA(ascii_char_ptr, DeviceId),
WITH_DATA(float, Temperature),
WITH_DATA(float, BatteryLevel),
WITH_DATA(int, DataPointsCount),
WITH_DATA(EDM_BINARY, FFTData)
);

END_NAMESPACE(MotorMonitor);

static char propText[1024];
/*
EXECUTE_COMMAND_RESULT TurnFanOn(ContosoAnemometer* device)
{
    (void)device;
    (void)printf("Turning fan on.\r\n");
    return EXECUTE_COMMAND_SUCCESS;
}

EXECUTE_COMMAND_RESULT TurnFanOff(ContosoAnemometer* device)
{
    (void)device;
    (void)printf("Turning fan off.\r\n");
    return EXECUTE_COMMAND_SUCCESS;
}

EXECUTE_COMMAND_RESULT SetAirResistance(ContosoAnemometer* device, int Position)
{
    (void)device;
    (void)printf("Setting Air Resistance Position to %d.\r\n", Position);
    return EXECUTE_COMMAND_SUCCESS;
}
*/
void sendCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result, void* userContextCallback)
{
    unsigned int messageTrackingId = (unsigned int)(uintptr_t)userContextCallback;

    (void)printf("Message Id: %u Received.\r\n", messageTrackingId);

    (void)printf("Result Call Back Called! Result is: %s \r\n", ENUM_TO_STRING(IOTHUB_CLIENT_CONFIRMATION_RESULT, result));
}

static void sendMessage(IOTHUB_CLIENT_LL_HANDLE iotHubClientHandle, const unsigned char* buffer, size_t size)
{
    static unsigned int messageTrackingId;
    IOTHUB_MESSAGE_HANDLE messageHandle = IoTHubMessage_CreateFromByteArray(buffer, size);
    if (messageHandle == NULL)
    {
        printf("unable to create a new IoTHubMessage\r\n");
    }
    else
    {
        if (IoTHubClient_LL_SendEventAsync(iotHubClientHandle, messageHandle, sendCallback, (void*)(uintptr_t)messageTrackingId) != IOTHUB_CLIENT_OK)
        {
            printf("failed to hand over the message to IoTHubClient");
        }
        else
        {
            printf("IoTHubClient accepted the message for delivery\r\n");
        }
        IoTHubMessage_Destroy(messageHandle);
    }
    free((void*)buffer);
    messageTrackingId++;
}

/*this function "links" IoTHub to the serialization library*/
static IOTHUBMESSAGE_DISPOSITION_RESULT IoTHubMessage(IOTHUB_MESSAGE_HANDLE message, void* userContextCallback)
{
    IOTHUBMESSAGE_DISPOSITION_RESULT result;
    const unsigned char* buffer;
    size_t size;
    if (IoTHubMessage_GetByteArray(message, &buffer, &size) != IOTHUB_MESSAGE_OK)
    {
        printf("unable to IoTHubMessage_GetByteArray\r\n");
        result = IOTHUBMESSAGE_ABANDONED;
    }
    else
    {
        /*buffer is not zero terminated*/
        char* temp = malloc(size + 1);
        if (temp == NULL)
        {
            printf("failed to malloc\r\n");
            result = IOTHUBMESSAGE_ABANDONED;
        }
        else
        {
            EXECUTE_COMMAND_RESULT executeCommandResult;
        
            (void)memcpy(temp, buffer, size);
            temp[size] = '\0';
            executeCommandResult = EXECUTE_COMMAND(userContextCallback, temp);
            result =
                (executeCommandResult == EXECUTE_COMMAND_ERROR) ? IOTHUBMESSAGE_ABANDONED :
                (executeCommandResult == EXECUTE_COMMAND_SUCCESS) ? IOTHUBMESSAGE_ACCEPTED :
                IOTHUBMESSAGE_REJECTED;
            free(temp);
        }
    }
    return result;
}

void simplesample_http_run(void)
{
    if (platform_init() != 0)
    {
        printf("Failed to initialize the platform.\r\n");
    }
    else
    {
        if (serializer_init(NULL) != SERIALIZER_OK)
        {
            (void)printf("Failed on serializer_init\r\n");
        }
        else
        {
            IOTHUB_CLIENT_LL_HANDLE iotHubClientHandle = IoTHubClient_LL_CreateFromConnectionString(connectionString, HTTP_Protocol);
            //int avgWindSpeed = 10;
            //float minTemperature = 20.0;
            //float minHumidity = 60.0;

            //srand((unsigned int)time(NULL));

            if (iotHubClientHandle == NULL)
            {
                (void)printf("Failed on IoTHubClient_LL_Create\r\n");
            }
            else
            {
                // Because it can poll "after 9 seconds" polls will happen 
                // effectively at ~10 seconds.
                // Note that for scalabilty, the default value of minimumPollingTime
                // is 25 minutes. For more information, see:
                // https://azure.microsoft.com/documentation/articles/iot-hub-devguide/#messaging
                unsigned int minimumPollingTime = 9;
                //ContosoAnemometer* myWeather;
                PdMMotorMonitor* motorMonitor;

                if (IoTHubClient_LL_SetOption(iotHubClientHandle, "MinimumPollingTime", &minimumPollingTime) != IOTHUB_CLIENT_OK)
                {
                    printf("failure to set option \"MinimumPollingTime\"\r\n");
                }

#ifdef MBED_BUILD_TIMESTAMP
                // For mbed add the certificate information
                if (IoTHubClient_LL_SetOption(iotHubClientHandle, "TrustedCerts", certificates) != IOTHUB_CLIENT_OK)
                {
                    (void)printf("failure to set option \"TrustedCerts\"\r\n");
                }
#endif // MBED_BUILD_TIMESTAMP

                motorMonitor = CREATE_MODEL_INSTANCE(MotorMonitor, PdMMotorMonitor);
                if (motorMonitor == NULL)
                {
                    (void)printf("Failed on CREATE_MODEL_INSTANCE\r\n");
                }
                else
                {
                    if (IoTHubClient_LL_SetMessageCallback(iotHubClientHandle, IoTHubMessage, motorMonitor) != IOTHUB_CLIENT_OK)
                    {
                        printf("unable to IoTHubClient_SetMessageCallback\r\n");
                    }
                    else
                    {
                        //myWeather->DeviceId = "myFirstDevice";
                        //myWeather->WindSpeed = avgWindSpeed + (rand() % 4 + 2);
                        //myWeather->Temperature = minTemperature + (rand() % 10);
                        //myWeather->Humidity = minHumidity + (rand() % 20);
                        //UInt32 seconds = 0; //Seconds_get();
                        //Log_info1("seconds = %u\n", (ULong)seconds);

                        //sl_DevSet(SL_DEVICE_GENERAL_CONFIGURATION,SL_DEVICE_GENERAL_CONFIGURATION_DATE_TIME,sizeof(SlDateTime_t),(unsigned char *) (&dateTime));
                        //sl_DevGet(SL_DEVICE_GENERAL_CONFIGURATION,&configOpt, &configLen,(unsigned char *)(&dateTime));

                        //printf("Day %d,Mon %d,Year %d,Hour %d,Min %d,Sec %d\n",dateTime.sl_tm_day,dateTime.sl_tm_mon,dateTime.sl_tm_year,dateTime.sl_tm_hour,dateTime.sl_tm_min,dateTime.sl_tm_sec);

                        //tm ltm = localtime(&seconds);
                        //char* currTime = asctime(ltm);

                       ///char buffer [80];
                        //strftime(buffer, 80, "%Y/%m/%dT%H:%M:%S", ltm);
                        unsigned char binaryArray[3] = { 0x01, 0x02, 0x03 };
                        EDM_BINARY binaryData = { sizeof(binaryArray), &binaryArray };
                        motorMonitor->Timestamp = "2018-03-18T11:02:56.1554105+00:00";
                        motorMonitor->TargetSite = "client01";
                        motorMonitor->DeviceId = "CC3220_board";
                        motorMonitor->Temperature = 72.0;
                        motorMonitor->BatteryLevel = 3.312;
                        motorMonitor->DataPointsCount = 3;
                        motorMonitor->FFTData = binaryData;
                        {
                            unsigned char* destination;
                            size_t destinationSize;
                            if (SERIALIZE(&destination, &destinationSize, motorMonitor->Timestamp, motorMonitor->TargetSite, motorMonitor->DeviceId, motorMonitor->Temperature, motorMonitor->BatteryLevel,
                                          motorMonitor->DataPointsCount, motorMonitor->FFTData) != CODEFIRST_OK)
 //                           if (SERIALIZE(&destination, &destinationSize, motorMonitor->Timestamp) != CODEFIRST_OK)
                            {
                                (void)printf("Failed to serialize\r\n");
                            }
                            else
                            {
                                IOTHUB_MESSAGE_HANDLE messageHandle = IoTHubMessage_CreateFromByteArray(destination, destinationSize);
                                if (messageHandle == NULL)
                                {
                                    printf("unable to create a new IoTHubMessage\r\n");
                                }
                                else
                                {
                                    /*
                                    MAP_HANDLE propMap = IoTHubMessage_Properties(messageHandle);
                                    (void)sprintf_s(propText, sizeof(propText), myWeather->Temperature > 28 ? "true" : "false");
                                    if (Map_AddOrUpdate(propMap, "temperatureAlert", propText) != MAP_OK)
                                    {
                                        printf("ERROR: Map_AddOrUpdate Failed!\r\n");
                                    }
                                    */
                                    if (IoTHubClient_LL_SendEventAsync(iotHubClientHandle, messageHandle, sendCallback, (void*)1) != IOTHUB_CLIENT_OK)
                                    {
                                        printf("failed to hand over the message to IoTHubClient");
                                    }
                                    else
                                    {
                                        printf("IoTHubClient accepted the message for delivery\r\n");
                                    }

                                    IoTHubMessage_Destroy(messageHandle);
                                }
                                free(destination);
                            }
                        }

                        /* wait for commands */
                        while (1)
                        {
                            IoTHubClient_LL_DoWork(iotHubClientHandle);
                            ThreadAPI_Sleep(100);
                        }
                    }

                    DESTROY_MODEL_INSTANCE(motorMonitor);
                }
                IoTHubClient_LL_Destroy(iotHubClientHandle);
            }
            serializer_deinit();
        }
        platform_deinit();
    }
}
