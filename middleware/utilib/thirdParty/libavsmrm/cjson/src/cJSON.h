/*
Portions of these materials are licensed under a proprietary license from Amazon and other portions are licensed under the MIT license below. 

*********************************************************
Amazon proprietary license

Copyright 2017-2018 Amazon.com, Inc. or its affiliates.  All Rights Reserved. 

A portion of these materials are licensed as "Program Materials" under the Program Materials License Agreement (the "Agreement") in connection with the Alexa Voice Service Program.  The Agreement is available at https://developer.amazon.com/public/support/pml.html.  See the Agreement for the specific terms and conditions of the Agreement.  Capitalized terms not defined in this file have the meanings given to them in the Agreement.  

In addition to the terms and conditions of the Agreement, the following terms and conditions apply to these materials.  In the event of a conflict between the terms and conditions of the Agreement and those of this file, the terms and conditions of this file will apply.

1.  These materials may only be used in connection with the development of Multi-Room Music functionality on Your Products. "Multi-Room Music" means the synchronized playback of content across multiple products that work with the Alexa Voice Service Program, and Alexa Voice Service means Amazons proprietary, cloud-based voice service referred to as Alexa, Alexa Voice Service, or AVS. 

2. If these materials are provided in source code form, you may modify or create derivative works of these materials, but only to the extent necessary to (a) develop, test and troubleshoot Your Products; or (b) port the materials to function on Your Products.  If you are a subcontractor who is using these materials on behalf of a third party, "Your Products" is restricted to the third party's products that work with the Alexa Voice Service Program.  

3.  These materials and modifications or derivatives thereof (collectively, "Licensed Materials") may be distributed in binary form only: (a) as pre-loaded on Your Products that include or facilitate access to the Alexa Voice Service; or (b) as included in a software update transmitted only to Your Products that include or facilitate access to the Alexa Voice Service.  

4.  These materials and all related documentation are Amazon confidential information (Confidential Information).  You agree that: (a) you will use Confidential Information only as is reasonably necessary for your use of these materials in accordance with the terms of the Agreement and this file; (b) you will not disclose Confidential Information to any individual, company, or other third party (except that the materials may be distributed in binary form as expressly provided in the Agreement and this file); and (c) you will take all reasonable measures to protect Confidential Information against any use or disclosure that is not expressly permitted by Amazon.


************************************************************
MIT License

   Copyright (c) 2009 Dave Gamble
 
   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:
 
   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.
 
   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
   THE SOFTWARE.

*/

#ifndef cJSON__h
#define cJSON__h

#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif


// cJSON Types:
#define cJSON_False 0
#define cJSON_True 1
#define cJSON_NULL 2
#define cJSON_Number 3
#define cJSON_String 4
#define cJSON_Array 5
#define cJSON_Object 6
/*indicates its an object, but lazy parsing was used such that it is still in original string form*/
#define cJSON_Object_As_String 7

#define cJSON_IsReference 256

// The cJSON structure:
typedef struct cJSON {
    struct cJSON *next,*prev;   // next/prev allow you to walk array/object chains. Alternatively, use GetArraySize/GetArrayItem/GetObjectItem
    struct cJSON *child;        // An array or object item will have a child pointer pointing to a chain of the items in the array/object.

    int type;                   // The type of the item, as above.

    char *valuestring;          // The item's string, if type==cJSON_String
    int valueint;               // The item's number, if type==cJSON_Number
    double valuedouble;         // The item's number, if type==cJSON_Number

    char *string;               // The item's name string, if this item is the child of, or is in the list of subitems of an object.
} cJSON;


/*
 * Gets bool value from cJSON object
 * object : cJSON object
 * string : key in object containing the bool value
 * pFound : (optional) if valid pointer will report if expected value was found
 */
extern bool cJSON_GetBoolItem(cJSON *object, const char *string, bool* pFound);

/*
 * Gets string value from cJSON object.
 * object : cJSON object
 * string : key in object containing the bool value
 * pFound : (optional) if valid pointer will report if expected value was found
 */
extern char* cJSON_GetStringItem(cJSON *object, const char *string, bool* pFound);

/*
 * Gets double value from cJSON object
 * object : cJSON object
 * string : key in object containing the bool value
 * pFound : (optional) if valid pointer will report if expected value was found
 */
extern double cJSON_GetDoubleItem(cJSON *object, const char *string, bool* pFound);

/*
 * Gets int value from cJSON object
 * object : cJSON object
 * string : key in object containing the bool value
 * pFound : (optional) if valid pointer will report if expected value was found
 */
extern int cJSON_GetIntItem(cJSON *object, const char *string, bool* pFound);

/*
 * Test type of JSON object
 */
extern bool cJSON_is_val_bool(cJSON* cjson);
extern bool cJSON_is_val_number(cJSON* cjson);
extern bool cJSON_is_val_string(cJSON* cjson);
extern bool cJSON_is_val_array(cJSON* cjson);
extern bool cJSON_is_val_object(cJSON* cjson);

/*
 * These getters differ from the cJSON_Get...
 * calls in that they assume you have already called
 * getObjectItem and now want to pull the value out.
 */
extern bool cJSON_get_bool_val(cJSON* cjson);
extern double cJSON_get_double_val(cJSON* cjson);
extern int cJSON_get_int_val(cJSON* cjson);
extern char* cJSON_get_string_val(cJSON* cjson);

typedef struct cJSON_Hooks {
      void *(*malloc_fn)(size_t sz);
      void (*free_fn)(void *ptr);
} cJSON_Hooks;

// Supply malloc, realloc and free functions to cJSON
extern void cJSON_InitHooks(cJSON_Hooks* hooks);


// Parse JSON in file, this returns a cJSON object you can interrogate. Call cJSON_Delete when finished
// useLazyParsing leaves sub-objects unparsed until you access them. If you have a large block of JSON
// and only intend on getting a couple of values out, lazy parsing will reduce the initial parsing time.
extern cJSON *cJSON_Parse_File(const char *filePathAndName, bool useLazyParsing);
// Supply a block of JSON, and this returns a cJSON object you can interrogate. Call cJSON_Delete when finished.
extern cJSON *cJSON_Parse(const char *value, bool useLazyParsing);
// Supply a block of JSON, and this returns a cJSON object you can interrogate. Call cJSON_Delete when finished.
//extern cJSON *cJSON_Parse_lazy(const char *value);
// Render a cJSON entity to text for transfer/storage. Free the char* when finished.
extern char  *cJSON_Print(cJSON *item);
// Render a cJSON entity to text for transfer/storage without any formatting. Free the char* when finished.
extern char  *cJSON_PrintUnformatted(cJSON *item);
// Delete a cJSON entity and all subentities.
extern void   cJSON_Delete(cJSON *c);

// Returns the number of items in an array (or object).
extern int    cJSON_GetArraySize(cJSON *array);
// Retrieve item number "item" from array "array". Returns NULL if unsuccessful.
extern cJSON *cJSON_GetArrayItem(cJSON *array,int item);
// Get item "string" from object. Case insensitive.
extern cJSON *cJSON_GetObjectItem(cJSON *object,const char *string);

// These calls create a cJSON item of the appropriate type.
extern cJSON *cJSON_CreateNull();
extern cJSON *cJSON_CreateTrue();
extern cJSON *cJSON_CreateFalse();
extern cJSON *cJSON_CreateNumber(double num);
extern cJSON *cJSON_CreateString(const char *string);
extern cJSON *cJSON_CreateArray();
extern cJSON *cJSON_CreateObject();

// These utilities create an Array of count items.
extern cJSON *cJSON_CreateIntArray(int *numbers,int count);
extern cJSON *cJSON_CreateFloatArray(float *numbers,int count);
extern cJSON *cJSON_CreateDoubleArray(double *numbers,int count);
extern cJSON *cJSON_CreateStringArray(const char **strings,int count);

// Append item to the specified array/object.
extern void cJSON_AddItemToArray(cJSON *array, cJSON *item);
extern void cJSON_AddItemToObject(cJSON *object,const char *string,cJSON *item);
// Append reference to item to the specified array/object. Use this when you want to add an existing cJSON to a new cJSON, but don't want to corrupt your existing cJSON.
extern void cJSON_AddItemReferenceToArray(cJSON *array, cJSON *item);
extern void cJSON_AddItemReferenceToObject(cJSON *object,const char *string,cJSON *item);

// Remove/Detatch items from Arrays/Objects.
extern cJSON *cJSON_DetachItemFromArray(cJSON *array,int which);
extern void   cJSON_DeleteItemFromArray(cJSON *array,int which);
extern cJSON *cJSON_DetachItemFromObject(cJSON *object,const char *string);
extern void   cJSON_DeleteItemFromObject(cJSON *object,const char *string);

// Update array items.
extern void cJSON_ReplaceItemInArray(cJSON *array,int which,cJSON *newitem);
extern void cJSON_ReplaceItemInObject(cJSON *object,const char *string,cJSON *newitem);

extern void cJSON_Minify(char *json);

#define cJSON_AddNullToObject(object,name)  cJSON_AddItemToObject(object, name, cJSON_CreateNull())
#define cJSON_AddTrueToObject(object,name)  cJSON_AddItemToObject(object, name, cJSON_CreateTrue())
#define cJSON_AddFalseToObject(object,name)     cJSON_AddItemToObject(object, name, cJSON_CreateFalse())
#define cJSON_AddNumberToObject(object,name,n)  cJSON_AddItemToObject(object, name, cJSON_CreateNumber(n))
#define cJSON_AddStringToObject(object,name,s)  cJSON_AddItemToObject(object, name, cJSON_CreateString(s))

#ifdef __cplusplus
}
#endif

#endif
