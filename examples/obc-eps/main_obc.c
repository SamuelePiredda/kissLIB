

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <conio.h>
#include <windows.h>

#include "kissLIB.h"



/* Write function for KISS framing */
int32_t write(kiss_instance_t *const kiss, const uint8_t *const data, size_t dataLen)
{
    /* tries to open the eps buffer */
    FILE *f = fopen("eps.txt", "wb+");
    if(NULL == f) 
    {
        return 1000;
    }
    /* write the message */
    fwrite(data, 1, dataLen, f);
    fclose(f);
    return 0;
}

/* Read function for KISS framing */
int32_t read(kiss_instance_t *const kiss, uint8_t *const buffer, size_t dataLen, size_t *const read)
{
    FILE *f;
    uint32_t i = 0;
    /* since the file can be accessed by both, to eliminate the chance that there is a collision
    * we try several times to open the file and only if we always fail it means that there is no message for us
    */
    do
    {
        /* wait for 5ms */
        Sleep(5);
        /* ty to open the file */
        f = fopen("obc.txt", "rb");
    } 
    while (NULL == f && i++ < *((uint32_t*)kiss->context));      
    /* if the file is still null it means we have no message to read */
    if(NULL == f)
    {
        return KISS_ERR_NO_DATA_RECEIVED;
    }
    /* if we are here there is a message that we read */
    *read = fread(buffer, 1, dataLen, f);
    fclose(f);
    /* remove the file */
    remove("obc.txt");
    return 0;
}

/* function to print the main menu and take a command as input */
void printMenu(int *const command)
{
    system("cls");
    printf("1. Reset EPS data\n");
    printf("2. Send data\n");
    printf("3. Get param\n");
    printf("4. Set param\n");
    printf("5. Get sensor\n");
    printf("Command: ");
    scanf("%d", command);
    return;
}

uint16_t selectParam()
{
    printf("Select param:\n");
    printf("PARAM1 (1)\n");
    printf("PARAM2 (2)\n");
    printf("PARAM3 (3)\n");
    printf("PARAM4 (4)\n");
    printf("Select param: ");
    int id = 0;
    scanf(" %d", &id);
    return (uint16_t)id;
}


uint16_t selectSensor()
{
    printf("Select sensor:\n");
    printf("1. SENS1 Voltage\n");
    printf("2. SENS2 Current\n");
    printf("3. SENS3 Temperature\n");
    int id = 0;
    scanf(" %d", &id);
    return (uint16_t)(id+4);
}


/** Example main function demonstrating KISS receive and decode.
 */
int main()
{
    /* buffer for KISS instance */
    uint8_t buffer[128];
    /* buffer for received data */
    uint8_t output[128];
    /* KISS header byte */
    uint8_t header;
    /* length of the output message */
    size_t len = 0;

    /* max time the program tries to open its own file buffer */
    uint32_t maxR = 10;

    /* kiss instance with the EPS */
    kiss_instance_t kiss_eps_i;

    /* error container for the EPS kiss instance */
    int kiss_err_eps = 0;

    // Initialize KISS instance
    kiss_err_eps = kiss_init(&kiss_eps_i, buffer, sizeof(buffer), 100, write, read, &maxR, 0);
    /* failed to initialized the kiss instance */
    if (kiss_err_eps != KISS_OK)
    {
        fprintf(stderr, "Failed to initialize KISS instance: %d\n", kiss_err_eps);
        return EXIT_FAILURE;
    }

    /* command that we will read from the user */
    int command;

    /* maximum amount of data that we will read from the user */
    char data[128];

    do
    {      
        /* print the main menu of the program and take a command as input */
        printMenu(&command);

        /* we switch for the command */
        switch(command)
        {
            /* case 1 we send the reset command to the EPS */
            case 1:

                /* we send the command which is 10 */
                kiss_err_eps = kiss_send_command(&kiss_eps_i, (uint16_t)10);
                /* if we had an error we print it */
                if(kiss_err_eps != KISS_OK)
                {
                    printf("Error sending command %d\n", kiss_err_eps);
                    return EXIT_FAILURE;
                }
                break;



            /* case 2 we send a new string to the EPS */
            case 2:
                /* new string */
                printf("New string: ");
                int c;
                /* clean the input buffer */
                while ((c = getchar()) != '\n' && c != EOF);
                /* initialize or reset the data array */
                for(int i = 0; i < 128; i++)
                    data[i] = 0;
                
                /* gets the string */
                fgets(data, 128, stdin);
                /* if we have some data */
                if(data != NULL)
                {

                    /* we encode and send the string at the data port 5*/
                    kiss_err_eps = kiss_encode_and_send(&kiss_eps_i, data, strlen(data), KISS_HEADER_DATA(5));

                    /* if we had an error */
                    if(kiss_err_eps != KISS_OK)
                    {
                        printf("Error sending data %d\n", kiss_err_eps);
                        return EXIT_FAILURE;
                    }
                }
                else
                {
                    /* no string has been read */
                    printf("Failed to get the new string\n");
                    return EXIT_FAILURE;
                }
                break;
            /* case 3 we ask for a specific param */
            case 3:
                /* selecting the parameter */
                uint16_t param_id = selectParam();

                /* if the parameter doesn't exist we just exit */
                if(param_id != 1 && param_id != 2 && param_id != 3 && param_id != 4)
                    break;

                /* send the request of the parameter */
                kiss_err_eps = kiss_request_param(&kiss_eps_i, param_id);

                /* if the request went ok we try to receive the packet */
                /* if the REAL CASE SCENARIO we should wait for the package to arrive but here we don't */
                if(kiss_err_eps == KISS_OK)
                {
                    /* receive and decode the parameter*/
                    kiss_err_eps = kiss_receive_frame(&kiss_eps_i, 1);
                    /* in case of errors */
                    if(kiss_err_eps != KISS_OK)
                    {
                        printf("Error during receiving parameter %d\n", kiss_err_eps);
                        return 1;
                    }

                    /* ID and value of the parameter that we received */
                    uint16_t id;
                    uint8_t value[8];

                    /* extract parameter and value */
                    kiss_err_eps = kiss_extract_param(&kiss_eps_i, &id, value, 8, &len);

                    /* error handling */
                    if(kiss_err_eps != KISS_OK)
                    {
                        printf("Error during extracting parameter %d\n", kiss_err_eps);
                        return 1;
                    }

                    /* just to make sure we check that the length of the parameter is 2 */
                    if(len != 2)
                    {
                        printf("The parameter received is not uint16_t %d\n", len);
                        return EXIT_FAILURE;
                    }

                    /* in this case we just have uint16_t */
                    /* in a real case scenario we will have different sizes for each parameter */
                    uint16_t val = KISS_BYTE_TO_UINT16(value[0], value[1]);

                    /* print the value and stop for a character from the user */
                    printf("PARAM%d Value: %d\n", id, val);
                    _getch();
                }
                else
                {
                    /* error handling, sending the request didn't go well */
                    printf("Error sending: %d\n", kiss_err_eps);
                    return EXIT_FAILURE;
                }
                break;
            
            /* in case 4 we want to change a parameter of the EPS */
            case 4:
                
                /* selecting the parameter */
                param_id = selectParam();

                /* value change */
                int value = 0;
                uint16_t param_value = 0;
                printf("Insert value: ");
                scanf(" %d", &value);
                param_value = (uint16_t)value;


                /* sending the parameter */
                kiss_err_eps = kiss_set_param(&kiss_eps_i, param_id, (uint8_t*)&param_value, 2);
                /* checking for errors */
                if(kiss_err_eps != KISS_OK)
                {
                    printf("Error during sending the set param %d\n", kiss_err_eps);
                    return 1;
                }
                break;
            /* case 5 we want to read a sensor */
            case 5:
                /* selecting the sensor to read */
                param_id = selectSensor();
                /* request the sensor which is the same as requesting the parameter */
                kiss_err_eps = kiss_request_param(&kiss_eps_i, param_id);

                if(kiss_err_eps != KISS_OK)
                {
                    printf("Error requesting a sensor value %d\n", kiss_err_eps);
                    return EXIT_FAILURE;
                }

                kiss_err_eps = kiss_receive_frame(&kiss_eps_i, 100);

                if(kiss_err_eps != KISS_OK)
                {
                    printf("Error receiving sensor frame %d\n", kiss_err_eps);
                    return EXIT_FAILURE;
                }

                uint16_t id;
                uint8_t sens_value[8];
                uint32_t sensor = 0;

                kiss_err_eps = kiss_extract_param(&kiss_eps_i, &id, sens_value, 8, &len);

                if(len != 4)
                {
                    printf("Error length of sensor %d is not ok %d\n", id, len);
                    return EXIT_FAILURE;
                }

                sensor = KISS_BYTE_TO_UINT32(sens_value[0], sens_value[1], sens_value[2], sens_value[3]);

                /* print the value and stop for a character from the user */
                printf("SENS%d Value: %d\n", id, sensor);
                _getch();

                break;
        }
        

    } while (1);


    
    return 0;
}
