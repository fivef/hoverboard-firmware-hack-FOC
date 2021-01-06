/**
  * This file is part of the hoverboard-firmware-hack project.
  *
  * Copyright (C) 2020-2021 Emanuel FERU <aerdronix@gmail.com>
  *
  * This program is free software: you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation, either version 3 of the License, or
  * (at your option) any later version.
  *
  * This program is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  * GNU General Public License for more details.
  *
  * You should have received a copy of the GNU General Public License
  * along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


// Includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stm32f1xx_hal.h"
#include "config.h"
#include "defines.h"
#include "eeprom.h"
#include "BLDC_controller.h"
#include "util.h"
#include "comms.h"

#if defined(DEBUG_SERIAL_PROTOCOL)

extern ExtY rtY_Left;                   /* External outputs */
extern ExtU rtU_Left;                   /* External inputs */
extern P    rtP_Left;

extern ExtY rtY_Right;                  /* External outputs */
extern ExtU rtU_Right;                      /* External inputs */
extern P    rtP_Right;


extern InputStruct input1[];            // input structure
extern InputStruct input2[];            // input structure

extern uint16_t VirtAddVarTab[NB_OF_VAR];
extern int16_t  speedAvg;                      // average measured speed
extern int16_t  speedAvgAbs;                   // average measured speed in absolute
extern uint8_t  ctrlModReqRaw;
extern adc_buf_t adc_buffer;
extern int16_t board_temp_deg_c;


enum commandTypes {READ,WRITE};
// Function0 - Function with 0 parameter
// Function1 - Function with 1 parameter (e.g. GET PARAM)
// Function2 - Function with 2 parameter (e.g. SET PARAM XXXX)
const command_entry commands[] = {
  // Type   ,Name      ,Function0         ,Function1       ,Function2      ,Help     
    {READ   ,"GET"     ,printAllParamDef  ,printParamDef   ,NULL           ,"Get Parameter/Variable"},
    {READ   ,"HELP"    ,printAllParamHelp ,printParamHelp  ,NULL           ,"Command/Parameter/Variable Help"},
    {READ   ,"WATCH"   ,NULL              ,watchParamVal   ,NULL           ,"Toggle Parameter/Variable Watch"},
    {WRITE  ,"SET"     ,NULL              ,NULL            ,setParamValExt ,"Set Parameter"},
    {WRITE  ,"INIT"    ,NULL              ,initParamVal    ,NULL           ,"Init Parameter from EEPROM or CONFIG.H"},
    {WRITE  ,"SAVE"    ,saveAllParamVal   ,NULL            ,NULL           ,"Save Parameters to EEPROM"},
};

enum paramTypes {PARAMETER,VARIABLE};
const parameter_entry params[] = {
  // CONTROL PARAMETERS
  // Type       ,Name                 ,Datatype ,ValueL ptr                  ,ValueR                    ,EEPRM Addr ,Init              ,Min    ,Max    ,Div             ,Mul  ,Fix   ,Callback Function  ,Help text
    {PARAMETER  ,"CTRL_MOD"           ,ADD_PARAM(ctrlModReqRaw)              ,NULL                      ,0          ,CTRL_MOD_REQ      ,1      ,3      ,0               ,0    ,0     ,NULL               ,"Ctrl mode 1:VLT 2:SPD 3:TRQ"},
    {PARAMETER  ,"CTRL_TYP"           ,ADD_PARAM(rtP_Left.z_ctrlTypSel)      ,&rtP_Right.z_ctrlTypSel   ,0          ,CTRL_TYP_SEL      ,0      ,2      ,0               ,0    ,0     ,NULL               ,"Ctrl type 0:COM 1:SIN 2:FOC"},
    {PARAMETER  ,"I_MOT_MAX"          ,ADD_PARAM(rtP_Left.i_max)             ,&rtP_Right.i_max          ,1          ,I_MOT_MAX         ,1      ,40     ,A2BIT_CONV      ,0    ,4     ,NULL               ,"Max phase current A"},
    {PARAMETER  ,"N_MOT_MAX"          ,ADD_PARAM(rtP_Left.n_max)             ,&rtP_Right.n_max          ,2          ,N_MOT_MAX         ,10     ,2000   ,0               ,0    ,4     ,NULL               ,"Max motor RPM"},
    {PARAMETER  ,"FI_WEAK_ENA"        ,ADD_PARAM(rtP_Left.b_fieldWeakEna)    ,&rtP_Right.b_fieldWeakEna ,0          ,FIELD_WEAK_ENA    ,0      ,1      ,0               ,0    ,0     ,NULL               ,"Enable field weak"},
  	{PARAMETER  ,"FI_WEAK_HI"         ,ADD_PARAM(rtP_Left.r_fieldWeakHi)     ,&rtP_Right.r_fieldWeakHi  ,0          ,FIELD_WEAK_HI     ,0      ,1500   ,0               ,0    ,4     ,Input_Lim_Init     ,"Field weak high RPM"},
	  {PARAMETER  ,"FI_WEAK_LO"         ,ADD_PARAM(rtP_Left.r_fieldWeakLo)     ,&rtP_Right.r_fieldWeakLo  ,0          ,FIELD_WEAK_LO     ,0      ,1000   ,0               ,0    ,4     ,Input_Lim_Init     ,"Field weak low RPM"},
    {PARAMETER  ,"FI_WEAK_MAX"        ,ADD_PARAM(rtP_Left.id_fieldWeakMax)   ,&rtP_Right.id_fieldWeakMax,0          ,FIELD_WEAK_MAX    ,0      ,20     ,A2BIT_CONV      ,0    ,4     ,NULL               ,"Field weak max current A(FOC)"},
    {PARAMETER  ,"PHA_ADV_MAX"        ,ADD_PARAM(rtP_Left.a_phaAdvMax)       ,&rtP_Right.a_phaAdvMax    ,0          ,PHASE_ADV_MAX     ,0      ,55     ,0               ,0    ,4     ,NULL               ,"Max Phase Adv angle Deg(SIN)"},     
  // INPUT PARAMETERS
  // Type       ,Name                 ,ValueL ptr                            ,ValueR                    ,EEPRM Addr ,Init              ,Min    ,Max    ,Div             ,Mul  ,Fix   ,Callback Function  ,Help text
    {PARAMETER  ,"PRI_IN1_TYP"        ,ADD_PARAM(input1[0].typ)              ,NULL                      ,3          ,0                 ,0      ,3      ,0               ,0    ,0     ,0                  ,"Input1 type"},        
    {PARAMETER  ,"PRI_IN1_MIN"        ,ADD_PARAM(input1[0].min)              ,NULL                      ,4          ,0                 ,0      ,0      ,0               ,0    ,0     ,0                  ,"Input1 min"},        
    {PARAMETER  ,"PRI_IN1_MID"        ,ADD_PARAM(input1[0].mid)              ,NULL                      ,5          ,0                 ,0      ,0      ,0               ,0    ,0     ,0                  ,"Input1 mid"},
    {PARAMETER  ,"PRI_IN1_MAX"        ,ADD_PARAM(input1[0].max)              ,NULL                      ,6          ,0                 ,0      ,0      ,0               ,0    ,0     ,0                  ,"Input1 max"},        
    {PARAMETER  ,"PRI_IN2_TYP"        ,ADD_PARAM(input2[0].typ)              ,NULL                      ,7          ,0                 ,0      ,3      ,0               ,0    ,0     ,0                  ,"Input2 type"},        
    {PARAMETER  ,"PRI_IN2_MIN"        ,ADD_PARAM(input2[0].min)              ,NULL                      ,8          ,0                 ,0      ,0      ,0               ,0    ,0     ,0                  ,"Input2 min"},        
    {PARAMETER  ,"PRI_IN2_MID"        ,ADD_PARAM(input2[0].mid)              ,NULL                      ,9          ,0                 ,0      ,0      ,0               ,0    ,0     ,0                  ,"Input2 mid"},
    {PARAMETER  ,"PRI_IN2_MAX"        ,ADD_PARAM(input2[0].max)              ,NULL                      ,10         ,0                 ,0      ,0      ,0               ,0    ,0     ,0                  ,"Input2 max"},
    {VARIABLE   ,"PRI_IN1_RAW"        ,ADD_PARAM(input1[0].raw)              ,NULL                      ,0          ,0                 ,0      ,0      ,0               ,0    ,0     ,0                  ,"Input1 raw"},        
    {VARIABLE   ,"PRI_IN2_RAW"        ,ADD_PARAM(input2[0].raw)              ,NULL                      ,0          ,0                 ,0      ,0      ,0               ,0    ,0     ,0                  ,"Input2 raw"},   
    {VARIABLE   ,"PRI_IN1_CMD"        ,ADD_PARAM(input1[0].cmd)              ,NULL                      ,0          ,0                 ,0      ,0      ,0               ,0    ,0     ,0                  ,"Input1 cmd"},        
    {VARIABLE   ,"PRI_IN2_CMD"        ,ADD_PARAM(input2[0].cmd)              ,NULL                      ,0          ,0                 ,0      ,0      ,0               ,0    ,0     ,0                  ,"Input2 cmd"},
#if defined(PRI_INPUT1) && defined(PRI_INPUT2) && defined(AUX_INPUT1) && defined(AUX_INPUT2)  
    {PARAMETER  ,"AUX_IN1_TYP"        ,ADD_PARAM(input1[1].typ)              ,NULL                      ,11         ,0                 ,0      ,3      ,0               ,0    ,0     ,0                  ,"Aux. input1 type"},        
    {PARAMETER  ,"AUX_IN1_MIN"        ,ADD_PARAM(input1[1].min)              ,NULL                      ,12         ,0                 ,0      ,0      ,0               ,0    ,0     ,0                  ,"Aux. input1 min"},        
    {PARAMETER  ,"AUX_IN1_MID"        ,ADD_PARAM(input1[1].mid)              ,NULL                      ,13         ,0                 ,0      ,0      ,0               ,0    ,0     ,0                  ,"Aux. input1 mid"},
    {PARAMETER  ,"AUX_IN1_MAX"        ,ADD_PARAM(input1[1].max)              ,NULL                      ,14         ,0                 ,0      ,0      ,0               ,0    ,0     ,0                  ,"Aux. input1 max"},        
    {PARAMETER  ,"AUX_IN2_TYP"        ,ADD_PARAM(input2[1].typ)              ,NULL                      ,15         ,0                 ,0      ,3      ,0               ,0    ,0     ,0                  ,"Aux. input2 type"},        
    {PARAMETER  ,"AUX_IN2_MIN"        ,ADD_PARAM(input2[1].min)              ,NULL                      ,16         ,0                 ,0      ,0      ,0               ,0    ,0     ,0                  ,"Aux. input2 min"},        
    {PARAMETER  ,"AUX_IN2_MID"        ,ADD_PARAM(input2[1].mid)              ,NULL                      ,17         ,0                 ,0      ,0      ,0               ,0    ,0     ,0                  ,"Aux. input2 mid"},
    {PARAMETER  ,"AUX_IN2_MAX"        ,ADD_PARAM(input2[1].max)              ,NULL                      ,18         ,0                 ,0      ,0      ,0               ,0    ,0     ,0                  ,"Aux. input2 max"},
    {VARIABLE   ,"AUX_IN1_RAW"        ,ADD_PARAM(input1[1].raw)              ,NULL                      ,0          ,0                 ,0      ,0      ,0               ,0    ,0     ,0                  ,"Aux. input1 raw"},        
    {VARIABLE   ,"AUX_IN2_RAW"        ,ADD_PARAM(input2[1].raw)              ,NULL                      ,0          ,0                 ,0      ,0      ,0               ,0    ,0     ,0                  ,"Aux. input2 raw"},        
    {VARIABLE   ,"AUX_IN1_CMD"        ,ADD_PARAM(input1[1].cmd)              ,NULL                      ,0          ,0                 ,0      ,0      ,0               ,0    ,0     ,0                  ,"Aux. input1 cmd"},        
    {VARIABLE   ,"AUX_IN2_CMD"        ,ADD_PARAM(input2[1].cmd)              ,NULL                      ,0          ,0                 ,0      ,0      ,0               ,0    ,0     ,0                  ,"Aux. input2 cmd"},
#endif  
  // FEEDBACK
  // Type       ,Name                 ,Datatype, ValueL ptr                  ,ValueR                    ,EEPRM Addr ,Init              ,Min    ,Max    ,Div             ,Mul  ,Fix   ,Callback Function  ,Help text
    {VARIABLE   ,"I_DC_LINK"          ,ADD_PARAM(rtU_Left.i_DCLink)          ,&rtU_Right.i_DCLink       ,0          ,0                 ,0      ,0      ,A2BIT_CONV      ,0    ,0     ,NULL               ,"DC Link current A"},
    {VARIABLE   ,"SPD_AVG"            ,ADD_PARAM(speedAvg)                   ,NULL                      ,0          ,0                 ,0      ,0      ,0               ,0    ,0     ,NULL               ,"Motor Measured Avg RPM"},
    {VARIABLE   ,"SPDL"               ,ADD_PARAM(rtY_Left.n_mot)             ,NULL                      ,0          ,0                 ,0      ,0      ,0               ,0    ,0     ,NULL               ,"Left Motor Measured RPM"},
    {VARIABLE   ,"SPDR"               ,ADD_PARAM(rtY_Right.n_mot)            ,NULL                      ,0          ,0                 ,0      ,0      ,0               ,0    ,0     ,NULL               ,"Right Motor Measured RPM"},
    {VARIABLE   ,"RATE"               ,0       , NULL                        ,NULL                      ,0          ,RATE              ,0      ,0      ,0               ,0    ,4     ,NULL               ,"Rate *10"},
    {VARIABLE   ,"SPD_COEF"           ,0       , NULL                        ,NULL                      ,0          ,SPEED_COEFFICIENT ,0      ,0      ,0               ,10   ,14    ,NULL               ,"Speed Coefficient *10"},
    {VARIABLE   ,"STR_COEF"           ,0       , NULL                        ,NULL                      ,0          ,STEER_COEFFICIENT ,0      ,0      ,0               ,10   ,14    ,NULL               ,"Steer Coefficient *10"},
    {VARIABLE   ,"BATV"               ,ADD_PARAM(adc_buffer.batt1)           ,NULL                      ,0          ,0                 ,0      ,0      ,0               ,0    ,0     ,NULL               ,"Battery voltage *100"},       
  //{VARIABLE   ,"TEMP"               ,ADD_PARAM(board_temp_deg_c)           ,NULL                      ,0          ,0                 ,0      ,0      ,0               ,0    ,0     ,NULL               ,"Temperature °C *10"},       

};

uint8_t * watchParamList;
uint8_t watchParamListSize = 0;

// Translate from External format to Internal Format
int32_t ExtToInt(uint8_t index,int32_t value){
  // Multiply to translate to internal format
  if(params[index].div) value *= params[index].div;
  // Shift to translate to internal format
  if (params[index].fix) value <<= params[index].fix;
  // Divide for small number
  if(params[index].mul) value /= params[index].mul;
  return value;
}

// Set Param with Value from external format
int8_t setParamValExt(uint8_t index, int32_t value) {   
  int8_t ret = 0;
  // check min and max before conversion to internal values
  if (IN_RANGE(value,params[index].min,params[index].max)){
    ret = setParamValInt(index,ExtToInt(index,value));
    printParamDef(index);
  }else{
    printf("! Value %li out of range [min:%li max:%li]\r\n",value,params[index].min,params[index].max);
  }
  return ret;
}

// Set Param with value from internal format
int8_t setParamValInt(uint8_t index, int32_t newValue) {
  int32_t value = newValue;
  if (*(int32_t*)params[index].valueL != value){ 
    // if value is different, beep, cast and assign new value
    switch (params[index].datatype){
      case UINT8_T:
        if (params[index].valueL != NULL) *(uint8_t*)params[index].valueL = value;
        if (params[index].valueR != NULL) *(uint8_t*)params[index].valueR = value;
        break;
      case UINT16_T:
        if (params[index].valueL != NULL) *(uint16_t*)params[index].valueL = value; 
        if (params[index].valueR != NULL) *(uint16_t*)params[index].valueR = value;
        break;
      case UINT32_T:
        if (params[index].valueL != NULL) *(uint32_t*)params[index].valueL = value; 
        if (params[index].valueR != NULL) *(uint32_t*)params[index].valueR = value;
        break;
      case INT8_T:
        if (params[index].valueL != NULL) *(int8_t*)params[index].valueL = value; 
        if (params[index].valueR != NULL) *(int8_t*)params[index].valueR = value;
        break;
      case INT16_T:
        if (params[index].valueL != NULL) *(int16_t*)params[index].valueL = value; 
        if (params[index].valueR != NULL) *(int16_t*)params[index].valueR = value;
        break;
      case INT32_T:
        if (params[index].valueL != NULL) *(int32_t*)params[index].valueL = value; 
        if (params[index].valueR != NULL) *(int32_t*)params[index].valueR = value;
        break;
    }
  }

  // Run callback function if assigned
  if (params[index].callback_function) (*params[index].callback_function)();
  return 1;
}

// Get Parameter Internal value and translate to external 
int32_t getParamValExt(uint8_t index) {
  return IntToExt(index,getParamValInt(index));
}

// Get Parameter Internal Value
int32_t getParamValInt(uint8_t index) {
  int32_t value = 0;

  int8_t countVar = 0;
  if (params[index].valueL != NULL) countVar++;
  if (params[index].valueR != NULL) countVar++;

  if (countVar > 0){
    // Read Left and Right values and calculate average 
    // If left and right have to be summed up, DIV field could be adapted to multiply by 2
    // Cast to parameter datatype
    switch (params[index].datatype){
      case UINT8_T:
        if (params[index].valueL != NULL) value += *(uint8_t*)params[index].valueL;
        if (params[index].valueR != NULL) value += *(uint8_t*)params[index].valueR;
        break;
      case UINT16_T:
        if (params[index].valueL != NULL) value += *(uint16_t*)params[index].valueL;
        if (params[index].valueR != NULL) value += *(uint16_t*)params[index].valueR;
        break;
      case UINT32_T:
        if (params[index].valueL != NULL) value += *(uint32_t*)params[index].valueL;
        if (params[index].valueR != NULL) value += *(uint32_t*)params[index].valueR;
        break;
      case INT8_T:
        if (params[index].valueL != NULL) value += *(int8_t*)params[index].valueL;
        if (params[index].valueR != NULL) value += *(int8_t*)params[index].valueR;
        break;
      case INT16_T:
        if (params[index].valueL != NULL) value += *(int16_t*)params[index].valueL;
        if (params[index].valueR != NULL) value += *(int16_t*)params[index].valueR;
        break;
      case INT32_T:
        if (params[index].valueL != NULL) value += *(int32_t*)params[index].valueL;
        if (params[index].valueR != NULL) value += *(int32_t*)params[index].valueR;
        break;
      default:
        value = 0;
    }

    // Divide by number of values provided for the parameter
    value /= countVar;
  }else{
    // No variable was provided, return init value that might contain a macro
    value = params[index].init;
  }

  return value;
}


// Set watch flag for parameter
int8_t watchParamVal(uint8_t index){
  boolean_T found = 0; 
  for(int i=0;i<watchParamListSize;i++){
    if (watchParamList[i] == index) found = 1;
    if ( found && i < watchParamListSize - 1 ) watchParamList[i] = watchParamList[i+1]; 
  }
  
  if (found){watchParamListSize--;}else{watchParamListSize++;}
  if (watchParamListSize == 0){
    free(watchParamList);
  }else{
    watchParamList = (uint8_t*) realloc(watchParamList, watchParamListSize * sizeof(uint8_t));
  }

  if (!found && watchParamList != NULL) watchParamList[watchParamListSize-1] = index;
  
  return 1;
}

// Print value for all parameters with watch flag
int8_t printParamVal(){
  if (watchParamList == NULL) return 0;
  for(int i=0;i<watchParamListSize;i++){
    printf("%s:%li ",params[watchParamList[i]].name,getParamValExt(watchParamList[i]));
  }
  if (watchParamListSize>0) printf("\r\n");
  return 1;
}

// Print help for parameter
int8_t printCommandHelp(uint8_t index){
  printf("? %s:%s\r\n",commands[index].name,commands[index].help);
  return 1;
}

// Print help for parameter
int8_t printParamHelp(uint8_t index){
  printf("? %s:%s ",params[index].name,params[index].help);
  if (params[index].type == PARAMETER) printf("[min:%li max:%li]",params[index].min,params[index].max);
  printf("\r\n");
  return 1;
}

// Print help for all parameters
int8_t printAllParamHelp(){
  printf("? Commands\r\n");
  for(int i=0;i<COMMAND_SIZE(commands);i++)
    printCommandHelp(i);
  printf("?\r\n");

  printf("? Parameters\r\n");
  for(int i=0;i<PARAM_SIZE(params);i++){
    if (params[i].type == PARAMETER) printParamHelp(i);
  }
  printf("?\r\n");

  printf("? Variables\r\n");
  for(int i=0;i<PARAM_SIZE(params);i++){
    if (params[i].type == VARIABLE) printParamHelp(i);
  }
  printf("?\r\n");

  return 1;
}

// Print definition for parameter
int8_t printParamDef(uint8_t index){
  printf("# name:%s value:%li init:%li min:%li max:%li\r\n",
         params[index].name,     // Parameter Name
         getParamValExt(index),  // Parameter Value translated to external format
         getParamInitExt(index), // Parameter Init Value translated to external format
         params[index].min,      // Parameter Min Value with External format 
         params[index].max);     // Parameter Max Value with External format
  return 1;
}

// Print definition for all parameters
int8_t printAllParamDef(){
  for(int i=0;i<PARAM_SIZE(params);i++) printParamDef(i);
  return 1;
}


// Get Parameter in External format, check max value, increment, set Parameter
int8_t incrParamVal(uint8_t index) {
  uint32_t value = getParamValExt(index);
  if (value < params[index].max){
    return setParamValExt(index,value + 1);
  }else{
    return setParamValExt(index,(int32_t) params[index].min);
  } 
}

// Get internal Parameter value and save it to EEprom for all paraemeter with an address assigned 
int8_t saveAllParamVal() {
  HAL_FLASH_Unlock();
  EE_WriteVariable(VirtAddVarTab[0] , (uint16_t)FLASH_WRITE_KEY);
  for(int i=0;i<PARAM_SIZE(params);i++){ 
    // Only Parameters with eeprom address can be saved
    if (params[i].addr){
      EE_WriteVariable(VirtAddVarTab[params[i].addr] , (uint16_t)getParamValInt(i));    
    }
  }
  HAL_FLASH_Lock();
  return 1;
}

int32_t IntToExt(uint8_t index,int32_t value){
  // Multiply for small number
  if(params[index].mul) value *= params[index].mul;
  // Divide to translate to external format
  if(params[index].div) value /= params[index].div;
  // Shift to translate to external format
  if(params[index].fix) value >>= params[index].fix;
  return value;
}

int32_t getParamInitExt(uint8_t index) {
  return IntToExt(index,getParamInitInt(index));
}

// Get Parameter value with EEprom data if address is avalaible, init value otherwise
int16_t getParamInitInt(uint8_t index){
  if (params[index].addr){
    // if EEPROM address is specified, init from EEPROM address
    uint16_t writeCheck, readVal;
    
    HAL_FLASH_Unlock();
    EE_ReadVariable(VirtAddVarTab[0], &writeCheck);
    EE_ReadVariable(VirtAddVarTab[params[index].addr] , &readVal);
    HAL_FLASH_Lock();
    
    // EEPROM was written, use stored value
    if (writeCheck == FLASH_WRITE_KEY){
      return readVal;
    }else{
      // Use init value from array
      return params[index].init;
    }
  }else{
    return params[index].init;
  }
}


// initialize Parameter value with EEprom data if address is avalaible, init value otherwise
int8_t initParamVal(uint8_t index) {
  int8_t ret = 0;
  ret = setParamValInt(index,(int32_t) getParamInitInt(index));
  printParamDef(index);
  return ret;
}

// Find parameter in params array and return index
int8_t findParam(uint8_t *userCommand, uint32_t len){
  for(int index=0;index<PARAM_SIZE(params);index++){
    uint8_t param_len = strlen(params[index].name);
    if (param_len < len){
      if (memcmp(userCommand,params[index].name,param_len)==0){
        return index;
      }
    }
  }
  return -1; // Not found
}

// Find command in commands array and return index
int8_t findCommand(uint8_t *userCommand, uint32_t len){
  for(int index=0;index<COMMAND_SIZE(commands);index++){
    uint8_t command_len = strlen(commands[index].name);
    if (command_len < len){
      if (memcmp(userCommand,commands[index].name,command_len)==0){
        return index;
      }
    }
  }
  return -1; // Not found
}


void handle_input(uint8_t *userCommand, uint32_t len)
{
  
  int8_t  cindex = -1;
  int8_t  pindex = -1;
  uint8_t size   = 0;
  int8_t  ret    = 0;

  // Find Command
  cindex = findCommand(userCommand,len);
  if (cindex == -1){
    printf("! Command not found\r\n");
    return;
  }

  // Skip command characters
  size = strlen(commands[cindex].name);
  {len-=size;userCommand+=size;}
  // Skip if space
  if (*userCommand == 0x20){len-=1;userCommand+=1;}

  if ( (*userCommand == '\n' || *userCommand == '\r') && 
       commands[cindex].callback_function0 != NULL){
    // This function needs no parameter
    ret = (*commands[cindex].callback_function0)();
    if (ret==1){printf("OK\r\n");}
    return;
  }

  // Find parameter
  pindex = findParam(userCommand,len);
  if (pindex == -1){
    printf("! Parameter not found\r\n");
    return;
  }
   
  if (commands[cindex].type == WRITE && params[pindex].type == VARIABLE){
    printf("! This command cannot be used with a Variable\r\n");
    return;
  }

  if (commands[cindex].callback_function1 != NULL){
    // This function needs only a parameter
    ret = (*commands[cindex].callback_function1)(pindex);
    if (ret==1){printf("OK\r\n");}
    return;
  }

  // Skip parameter characters
  size = strlen(params[pindex].name);
  {len-=size;userCommand+=size;}
  // Skip if space
  if (*userCommand == 0x20){len-=1;userCommand+=1;}

  
  int32_t value = 0;
  int8_t  sign  = 1;
  int8_t  count = 0;

  // Read sign
  if (*userCommand == '-'){len-=1;userCommand+=1;sign =-1;} 
  // Read value
  for (value=0; (unsigned)*userCommand-'0'<10; userCommand++){
    value=10*value+(*userCommand-'0');
    count++;
    if (value>MAX_int16_T){printf("! Value not in range\r\n");return;}
  }

  if (count == 0){
    printf("! Value required\r\n");
    return;
  }
      
  // Apply sign
  value*= sign;

  if (commands[cindex].callback_function2 != NULL){
    // This function needs an additional parameter
    ret = (*commands[cindex].callback_function2)(pindex,value);
    if (ret==1){printf("OK\r\n");}
  }

}

#endif