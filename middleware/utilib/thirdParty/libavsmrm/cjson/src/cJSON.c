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
// cJSON
// JSON parser in C.
 
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <float.h>
#include <limits.h>
#include <ctype.h>
#include "cJSON.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <sys/mman.h>

#include <fcntl.h>
static int cJSON_strcasecmp(const char *s1, const char *s2) {
    if (!s1)
        return (s1 == s2) ? 0 : 1;
    if (!s2)
        return 1;
    for (; tolower(*s1) == tolower(*s2); ++s1, ++s2)
        if (*s1 == 0)
            return 0;
    return tolower(*(const unsigned char *) s1) - tolower(
            *(const unsigned char *) s2);
}

static void *(*cJSON_malloc)(size_t sz) = malloc;
static void (*cJSON_free)(void *ptr) = free;

static char* cJSON_strdup(const char* str) {
    size_t len;
    char* copy;

    len = strlen(str) + 1;
    if (!(copy = (char*) cJSON_malloc(len)))
        return 0;
    memcpy(copy, str, len);
    return copy;
}

void cJSON_InitHooks(cJSON_Hooks* hooks) {
    if (!hooks) { /* Reset hooks */
        cJSON_malloc = malloc;
        cJSON_free = free;
        return;
    }

    cJSON_malloc = (hooks->malloc_fn) ? hooks->malloc_fn : malloc;
    cJSON_free = (hooks->free_fn) ? hooks->free_fn : free;
}

// Internal constructor.
static cJSON *cJSON_New_Item() {
    cJSON* node = (cJSON*) cJSON_malloc(sizeof(cJSON));
    if (node)
        memset(node, 0, sizeof(cJSON));
    return node;
}

// Delete a cJSON structure.
void cJSON_Delete(cJSON *c) {
    cJSON *next;
    while (c) {
        next = c->next;
        if (!(c->type & cJSON_IsReference) && c->child)
            cJSON_Delete(c->child);
        if (!(c->type & cJSON_IsReference) && c->valuestring)
            cJSON_free(c->valuestring);
        if (c->string)
            cJSON_free(c->string);
        cJSON_free(c);
        c = next;
    }
}

// Parse the input text to generate a number, and populate the result into item.
static const char *parse_number(cJSON *item, const char *num) {
    double n = 0, sign = 1, scale = 0;
    int subscale = 0, signsubscale = 1;

    // Could use sscanf for this?
    if (*num == '-')
        sign = -1, num++; // Has sign?
    if (*num == '0')
        num++; // is zero
    if (*num >= '1' && *num <= '9')
        do
            n = (n * 10.0) + (*num++ - '0');
        while (*num >= '0' && *num <= '9'); // Number?
    if (*num == '.') {
        num++;
        do
            n = (n * 10.0) + (*num++ - '0'), scale--;
        while (*num >= '0' && *num <= '9');
    } // Fractional part?
    if (*num == 'e' || *num == 'E') // Exponent?
    {
        num++;
        if (*num == '+')
            num++;
        else if (*num == '-')
            signsubscale = -1, num++; // With sign?
        while (*num >= '0' && *num <= '9')
            subscale = (subscale * 10) + (*num++ - '0'); // Number?
    }

    n = sign * n * pow(10.0, (scale + subscale * signsubscale)); // number = +/- number.fraction * 10^+/- exponent

    item->valuedouble = n;
    item->valueint = (int) n;
    item->type = cJSON_Number;
    return num;
}

// Render the number nicely from the given item into a string.
static char *print_number(cJSON *item) {
    char *str;
    double d = item->valuedouble;
    if (fabs(((double) item->valueint) - d) <= DBL_EPSILON && d <= INT_MAX && d
            >= INT_MIN) {
        str = (char*) cJSON_malloc(21); // 2^64+1 can be represented in 21 chars.
        if (!str)
            return 0; /* memory fail */
        sprintf(str, "%d", item->valueint);
    } else {
        str = (char*) cJSON_malloc(64); // This is a nice tradeoff.
        if (!str)
            return 0; /* memory fail */
        if (fabs(floor(d) - d) <= DBL_EPSILON)
            sprintf(str, "%.0f", d);
        else if (fabs(d) < 1.0e-6 || fabs(d) > 1.0e9)
            sprintf(str, "%e", d);
        else
            sprintf(str, "%f", d);
    }
    return str;
}

// Parse the input text into an unescaped cstring, and populate item.
static const char
        firstByteMark[7] = { 0x00, 0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC };

static const char *parse_string(cJSON *item, const char *str) {
    const char *ptr = str + 1;
    char *ptr2;
    char *out;
    int len = 0;
    // length of input string
    int length = strlen(str);
    const char* endPtr = str + length;
    unsigned uc;
    if (*str != '\"')
        return 0; // not a string!

    /*
     * In order to have any memory overwrite issue, allocate the output string to be the same as
     * input string. This would eliminate any buffer overwrite issues
     */
    out = (char*) cJSON_malloc(strlen(str) + 1);
    if (!out)
        return 0;

    ptr = str + 1;
    ptr2 = out;
    while (*ptr != '\"' && (unsigned char) *ptr > 31 && ptr < endPtr) {
        if (*ptr != '\\')
            *ptr2++ = *ptr++;
        else {
            ptr++;
            switch (*ptr) {
            case 'b':
                *ptr2++ = '\b';
                break;
            case 'f':
                *ptr2++ = '\f';
                break;
            case 'n':
                *ptr2++ = '\n';
                break;
            case 'r':
                *ptr2++ = '\r';
                break;
            case 't':
                *ptr2++ = '\t';
                break;
            case 'u': // transcode utf16 to utf8. DOES NOT SUPPORT SURROGATE PAIRS CORRECTLY.
                if (sscanf(ptr + 1, "%4x", &uc) == 1) { // get the unicode char.
                    len = 3;
                    if (uc < 0x80)
                        len = 1;
                    else if (uc < 0x800)
                        len = 2;
                    ptr2 += len;

                    switch (len) {
                    case 3:
                        *--ptr2 = ((uc | 0x80) & 0xBF);
                        uc >>= 6;
                    case 2:
                        *--ptr2 = ((uc | 0x80) & 0xBF);
                        uc >>= 6;
                    case 1:
                        *--ptr2 = (uc | firstByteMark[len]);
                    }
                    ptr2 += len;
                    ptr += 4;
                    if (ptr >= endPtr) {
                        return 0;
                    }
                } else {
                    // sscanf failed, just move forward and continue
                    ptr++;
                }
                break;
            default:
                *ptr2++ = *ptr;
                break;
            }
            ptr++;
            if (ptr >= endPtr) {
                return 0;
            }
        }
    }
    *ptr2 = 0;
    if (*ptr == '\"')
        ptr++;
    item->valuestring = out;
    item->type = cJSON_String;
    return ptr;
}

// Render the cstring provided to an escaped version that can be printed.
static char *print_string_ptr(const char *str) {
    const char *ptr;
    char *ptr2, *out;
    int len = 0;

    if (!str)
        return cJSON_strdup("");
    ptr = str;
    while (*ptr && ++len) {
        if ((unsigned char) *ptr < 32 || *ptr == '\"' || *ptr == '\\')
            len++;
        ptr++;
    }

    out = (char*) cJSON_malloc(len + 3);
    if (!out)
        return 0; /* memory fail */
    ptr2 = out;
    ptr = str;
    *ptr2++ = '\"';
    while (*ptr) {
        if ((unsigned char) *ptr > 31 && *ptr != '\"' && *ptr != '\\')
            *ptr2++ = *ptr++;
        else {
            *ptr2++ = '\\';
            switch (*ptr++) {
            case '\\':
                *ptr2++ = '\\';
                break;
            case '\"':
                *ptr2++ = '\"';
                break;
            case '\b':
                *ptr2++ = 'b';
                break;
            case '\f':
                *ptr2++ = 'f';
                break;
            case '\n':
                *ptr2++ = 'n';
                break;
            case '\r':
                *ptr2++ = 'r';
                break;
            case '\t':
                *ptr2++ = 't';
                break;
            default:
                ptr2--;
                break; // eviscerate with prejudice.
            }
        }
    }
    *ptr2++ = '\"';
    *ptr2++ = 0;
    return out;
}
// Invote print_string_ptr (which is useful) on an item.
static char *print_string(cJSON *item) {
    return print_string_ptr(item->valuestring);
}

// Predeclare these prototypes.
static const char *parse_value(cJSON *item, const char *value,
        bool leaveObjectsAsStrings);
static char *print_value(cJSON *item, int depth, int fmt);
static const char *parse_array(cJSON *item, const char *value,
        bool leaveObjectsAsStrings);
static char *print_array(cJSON *item, int depth, int fmt);
static const char *parse_object(cJSON *item, const char *value,
        bool leaveObjectsAsStrings);
static const char *parse_object_as_string(cJSON *item, const char *value);
static char *print_object(cJSON *item, int depth, int fmt);

// Utility to jump whitespace and cr/lf
static const char *skip(const char *in) {

    while (in && *in && (unsigned char) *in <= 32)
        in++;

    return in;
}

// Parse an object - create a new root, and populate.
cJSON *cJSON_Parse_File(const char *filePathAndName, bool useLazyParsing) {

    int fd = open(filePathAndName, O_RDONLY);

    if (fd == -1) {
        return 0;
    }

    struct stat statFile;

    /* if statFile.st_size is 0 bail */
    if (0 != fstat(fd, &statFile) || 0 == statFile.st_size) {
        close(fd);
        return 0;
    }

    /* allocate an additional byte past the file so we can null terminate
     * the string. Space past file should be 0 by default*/
    char* pBuf = (char*) mmap(0, statFile.st_size + 1, PROT_READ,
            MAP_PRIVATE, fd, 0);

    if (pBuf == MAP_FAILED) {
        close(fd);
        return 0;
    }

    cJSON *c = cJSON_Parse(pBuf, useLazyParsing);

    munmap(pBuf, statFile.st_size + 1);

    close(fd);

    return c;
}

// Parse an object - create a new root, and populate.
cJSON *cJSON_Parse(const char *value, bool useLazyParsing) {
    cJSON *c = cJSON_New_Item();
    if (!c)
        return 0; /* memory fail */

    if (!parse_value(c, skip(value), useLazyParsing)) {
        cJSON_Delete(c);
        return 0;
    }
    return c;
}

// Render a cJSON item/entity/structure to text.
char *cJSON_Print(cJSON *item) {
    return print_value(item, 0, 1);
}
char *cJSON_PrintUnformatted(cJSON *item) {
    return print_value(item, 0, 0);
}

// Parser core - when encountering text, process appropriately.
static const char *parse_value(cJSON *item, const char *value,
        bool leaveObjectsAsStrings) {


    if (!value)
        return 0; // Fail on null.
    if (!strncmp(value, "null", 4)) {
        item->type = cJSON_NULL;
        return value + 4;
    }
    if (!strncmp(value, "false", 5)) {
        item->type = cJSON_False;
        return value + 5;
    }
    if (!strncmp(value, "true", 4)) {
        item->type = cJSON_True;
        item->valueint = 1;
        return value + 4;
    }
    if (*value == '\"') {
        return parse_string(item, value);
    }
    if (*value == '-' || (*value >= '0' && *value <= '9')) {
        return parse_number(item, value);
    }
    if (*value == '[') {
        return parse_array(item, value, leaveObjectsAsStrings);
    }
    if (*value == '{') {
        if (leaveObjectsAsStrings) {
            return parse_object_as_string(item, value);
        } else {
            return parse_object(item, value, false);
        }
    }

    return 0; // failure.
}

static char* print_object_as_string(cJSON* item, int depth, int fmt) {
    //for now we dont ever pretty up these so depth and format are not needed
    int len = strlen(item->valuestring);
    char* out = (char*) cJSON_malloc(len + 1);
    if (!out)
        return 0; /* memory fail */
    strcpy(out, item->valuestring);

    return out;
}

// Render a value to text.
static char *print_value(cJSON *item, int depth, int fmt) {
    char *out = 0;
    if (!item)
        return 0;
    switch ((item->type) & 255) {
    case cJSON_NULL:
        out = cJSON_strdup("null");
        break;
    case cJSON_False:
        out = cJSON_strdup("false");
        break;
    case cJSON_True:
        out = cJSON_strdup("true");
        break;
    case cJSON_Number:
        out = print_number(item);
        break;
    case cJSON_String:
        out = print_string(item);
        break;
    case cJSON_Array:
        out = print_array(item, depth, fmt);
        break;
    case cJSON_Object:
        out = print_object(item, depth, fmt);
        break;
    case cJSON_Object_As_String:
        out = print_object_as_string(item, depth, fmt);
        break;
    }
    return out;
}

// Build an array from input text.
static const char *parse_array(cJSON *item, const char *value,
        bool leaveObjectsAsStrings) {
    cJSON *child;
    if (*value != '[')
        return 0; // not an array!

    item->type = cJSON_Array;
    value = skip(value + 1);
    if (*value == ']')
        return value + 1; // empty array.

    item->child = child = cJSON_New_Item();
    if (!item->child)
        return 0; // memory fail
    value = skip(parse_value(child, skip(value), leaveObjectsAsStrings)); // skip any spacing, get the value.
    if (!value)
        return 0;

    while (*value == ',') {
        cJSON *new_item;
        if (!(new_item = cJSON_New_Item()))
            return 0; // memory fail
        child->next = new_item;
        new_item->prev = child;
        child = new_item;
        value
                = skip(parse_value(child, skip(value + 1),
                        leaveObjectsAsStrings));
        if (!value)
            return 0; // memory fail
    }

    if (*value == ']')
        return value + 1; // end of array
    return 0; // malformed.
}

// Render an array to text
static char *print_array(cJSON *item, int depth, int fmt) {
    char **entries;
    char *out = 0, *ptr, *ret;
    int len = 5;
    cJSON *child = item->child;
    int numentries = 0, i = 0, fail = 0;

    // How many entries in the array?
    while (child)
        numentries++, child = child->next;
    // Allocate an array to hold the values for each
    entries = (char**) cJSON_malloc(numentries * sizeof(char*));
    if (!entries)
        return 0;
    memset(entries, 0, numentries * sizeof(char*));
    // Retrieve all the results:
    child = item->child;
    while (child && !fail) {
        ret = print_value(child, depth + 1, fmt);
        entries[i++] = ret;
        if (ret)
            len += strlen(ret) + 2 + (fmt ? 1 : 0);
        else
            fail = 1;
        child = child->next;
    }

    // If we didn't fail, try to malloc the output string
    if (!fail)
        out = (char *) cJSON_malloc(len);
    // If that fails, we fail.
    if (!out)
        fail = 1;

    // Handle failure.
    if (fail) {
        for (i = 0; i < numentries; i++)
            if (entries[i])
                cJSON_free(entries[i]);
        cJSON_free(entries);
        return 0;
    }

    // Compose the output array.
    *out = '[';
    ptr = out + 1;
    *ptr = 0;
    for (i = 0; i < numentries; i++) {
        strcpy(ptr, entries[i]);
        ptr += strlen(entries[i]);
        if (i != numentries - 1) {
            *ptr++ = ',';
            if (fmt)
                *ptr++ = ' ';
            *ptr = 0;
        }
        cJSON_free(entries[i]);
    }
    cJSON_free(entries);
    *ptr++ = ']';
    *ptr++ = 0;
    return out;
}

// Build an object from the text.
static const char *parse_object(cJSON *item, const char *value,
        bool leaveObjectsAsStrings) {
    cJSON *child;
    if (*value != '{')
        return 0; // not an object!

    item->type = cJSON_Object;
    value = skip(value + 1);
    if (*value == '}')
        return value + 1; // empty array.

    item->child = child = cJSON_New_Item();
    value = skip(parse_string(child, skip(value)));
    if (!value)
        return 0;
    child->string = child->valuestring;
    child->valuestring = 0;
    if (*value != ':')
        return 0; // fail!
    value = skip(parse_value(child, skip(value + 1), leaveObjectsAsStrings)); // skip any spacing, get the value.
    if (!value)
        return 0;

    while (*value == ',') {
        cJSON *new_item;
        if (!(new_item = cJSON_New_Item()))
            return 0; // memory fail
        child->next = new_item;
        new_item->prev = child;
        child = new_item;
        value = skip(parse_string(child, skip(value + 1)));
        if (!value)
            return 0;
        child->string = child->valuestring;
        child->valuestring = 0;
        if (*value != ':')
            return 0; // fail!
        value
                = skip(parse_value(child, skip(value + 1),
                        leaveObjectsAsStrings)); // skip any spacing, get the value.
        if (!value)
            return 0;
    }

    if (*value == '}')
        return value + 1; // end of array
    return 0; // malformed.
}

static const char *parse_object_as_string(cJSON *item, const char *str) {
    const char *ptr = str + 1;
    char* out;
    int len = 1;

    if (*str != '{')
        return 0; // not object!

    /*figure out the length*/
    int openCloseCount = 1;
    int inString = 0;
    while (openCloseCount > 0) {
        len++;
        if (inString) {
            if (*ptr == '"') {
                // Reached the end of the quoted string.
                inString = 0;
            } else if (*ptr == '\\') {
                // We are skipping over an escaped character.  We don't care
                // what it is. (Other parts of this library handle escapes the
                // same.)
                //
                // The loop always moves us forward one character, so we just
                // need to move forward one extra.
                len++;
                ptr++;
                if (*ptr == 0) {
                    // The escaping backslash is actually the last character in
                    // the string, so it has nothing to escape.
                    // Return parse error.
                    return 0;
                }
            } else if (*ptr == 0) {
                // Reached end-of-input before the closing quote of the string.
                // Return parse error.
                return 0;
            }
        }
        else {
            if (*ptr == '"') {
                // At the beginning of a quoted string.
                inString = 1;
            } else if (*ptr == '{') {
                openCloseCount++;
            } else if (*ptr == '}') {
                openCloseCount--;
            } else if (*ptr == 0) {
                return 0;
            }
        }

        //iterate the ptr to the next char
        ptr++;
    }

    out = (char*) cJSON_malloc(len + 1); // This is how long we need for the string, roughly.
    if (!out)
        return 0;

    out = strncpy(out, str, len);
    out[len] = 0;
    item->valuestring = out;
    item->type = cJSON_Object_As_String;

    return ptr;
}

// Render an object to text.
static char *print_object(cJSON *item, int depth, int fmt) {
    char **entries = 0, **names = 0;
    char *out = 0, *ptr, *ret, *str;
    int len = 7, i = 0, j;
    cJSON *child = item->child;
    int numentries = 0, fail = 0;
    // Count the number of entries.
    while (child)
        numentries++, child = child->next;
    // Allocate space for the names and the objects
    entries = (char**) cJSON_malloc(numentries * sizeof(char*));
    if (!entries)
        return 0;
    names = (char**) cJSON_malloc(numentries * sizeof(char*));
    if (!names) {
        cJSON_free(entries);
        return 0;
    }
    memset(entries, 0, sizeof(char*) * numentries);
    memset(names, 0, sizeof(char*) * numentries);

    // Collect all the results into our arrays:
    child = item->child;
    depth++;
    if (fmt)
        len += depth;
    while (child) {
        names[i] = str = print_string_ptr(child->string);
        entries[i++] = ret = print_value(child, depth, fmt);
        if (str && ret)
            len += strlen(ret) + strlen(str) + 2 + (fmt ? 2 + depth : 0);
        else
            fail = 1;
        child = child->next;
    }

    // Try to allocate the output string
    if (!fail)
        out = (char*) cJSON_malloc(len);
    if (!out)
        fail = 1;

    // Handle failure
    if (fail) {
        for (i = 0; i < numentries; i++) {
            if (names[i])
                free(names[i]);
            if (entries[i])
                free(entries[i]);
        }
        free(names);
        free(entries);
        return 0;
    }

    // Compose the output:
    *out = '{';
    ptr = out + 1;
    if (fmt)
        *ptr++ = '\n';
    *ptr = 0;
    for (i = 0; i < numentries; i++) {
        if (fmt)
            for (j = 0; j < depth; j++)
                *ptr++ = '\t';
        strcpy(ptr, names[i]);
        ptr += strlen(names[i]);
        *ptr++ = ':';
        if (fmt)
            *ptr++ = '\t';
        strcpy(ptr, entries[i]);
        ptr += strlen(entries[i]);
        if (i != numentries - 1)
            *ptr++ = ',';
        if (fmt)
            *ptr++ = '\n';
        *ptr = 0;
        cJSON_free(names[i]);
        cJSON_free(entries[i]);
    }

    cJSON_free(names);
    cJSON_free(entries);
    if (fmt)
        for (i = 0; i < depth - 1; i++)
            *ptr++ = '\t';
    *ptr++ = '}';
    *ptr++ = 0;
    return out;
}

// Get Array size/item / object item.
int cJSON_GetArraySize(cJSON *array) {
    cJSON *c = array->child;
    int i = 0;
    while (c)
        i++, c = c->next;
    return i;
}
cJSON *cJSON_GetArrayItem(cJSON *array, int item) {
    cJSON *c = array->child;
    while (c && item > 0)
        item--, c = c->next;
    return c;
}

cJSON *cJSON_GetObjectItem(cJSON *object, const char *string) {

    if (object->type == cJSON_Object_As_String) {
        //this is an "unparsed" object we need to parse it and swap it in
        if (0 == parse_object(object, object->valuestring, true)){
            /*failed to parse*/
            return NULL;
        }
        cJSON_free(object->valuestring);
        object->valuestring = NULL;
    }

    cJSON *c = object->child;

    while (c && cJSON_strcasecmp(c->string, string)) {
        c = c->next;
    }

    return c;
}

bool cJSON_GetBoolItem(cJSON *object, const char *string, bool* pFound) {
    cJSON* item = cJSON_GetObjectItem(object, string);

    bool returnVal = false;
    bool found = false;

    if (cJSON_is_val_bool(item)) {

        found = true;
        if (item->type == cJSON_False) {
            returnVal = false;
        } else {
            returnVal = true;
        }
    }

    if (pFound) {
        *pFound = found;
    }

    return returnVal;
}

char* cJSON_GetStringItem(cJSON *object, const char *string, bool* pFound) {
    cJSON* item = cJSON_GetObjectItem(object, string);

    char* returnVal = NULL;
    bool found = false;

    if (cJSON_is_val_string(item)) {
        found = true;
        returnVal = item->valuestring;
    }

    if (pFound) {
        *pFound = found;
    }

    return returnVal;
}

double cJSON_GetDoubleItem(cJSON *object, const char *string, bool* pFound) {
    cJSON* item = cJSON_GetObjectItem(object, string);

    double returnVal = 0;
    bool found = false;

    if (cJSON_is_val_number(item)) {
        found = true;
        returnVal = item->valuedouble;
    }

    if (pFound) {
        *pFound = found;
    }

    return returnVal;
}

int cJSON_GetIntItem(cJSON *object, const char *string, bool* pFound) {
    cJSON* item = cJSON_GetObjectItem(object, string);

    int returnVal = 0;
    bool found = false;

    if (cJSON_is_val_number(item)) {
        found = true;
        returnVal = item->valueint;
    }

    if (pFound) {
        *pFound = found;
    }

    return returnVal;
}

// Utility for array list handling.
static void suffix_object(cJSON *prev, cJSON *item) {
    prev->next = item;
    item->prev = prev;
}
// Utility for handling references.
static cJSON *create_reference(cJSON *item) {
    cJSON *ref = cJSON_New_Item();
    if (!ref)
        return 0; /* memory fail */
    memcpy(ref, item, sizeof(cJSON));
    ref->string = 0;
    ref->type |= cJSON_IsReference;
    ref->next = ref->prev = 0;
    return ref;
}

// Add item to array/object.
void cJSON_AddItemToArray(cJSON *array, cJSON *item) {
    if ((!array) || (!item)) {
        return; /* can not add */
    }
    cJSON *c = array->child;
    if (!c) {
        array->child = item;
    } else {
        while (c && c->next)
            c = c->next;
        suffix_object(c, item);
    }
}
void cJSON_AddItemToObject(cJSON *object, const char *string, cJSON *item) {
    if ((!object) || (!item) || (!string)) {
        return; /* can not add */
    }
    if (item->string)
        cJSON_free(item->string);
    item->string = cJSON_strdup(string);
    cJSON_AddItemToArray(object, item);
}
void cJSON_AddItemReferenceToArray(cJSON *array, cJSON *item) {
    cJSON_AddItemToArray(array, create_reference(item));
}
void cJSON_AddItemReferenceToObject(cJSON *object, const char *string,
        cJSON *item) {
    cJSON_AddItemToObject(object, string, create_reference(item));
}

cJSON *cJSON_DetachItemFromArray(cJSON *array, int which) {
    if (!array) {
        return 0;
    }
    cJSON *c = array->child;
    while (c && which > 0)
        c = c->next, which--;
    if (!c)
        return 0;
    if (c->prev)
        c->prev->next = c->next;
    if (c->next)
        c->next->prev = c->prev;
    if (c == array->child)
        array->child = c->next;
    c->prev = c->next = 0;
    return c;
}
void cJSON_DeleteItemFromArray(cJSON *array, int which) {
    cJSON_Delete(cJSON_DetachItemFromArray(array, which));
}
cJSON *cJSON_DetachItemFromObject(cJSON *object, const char *string) {
    if ((!object) || (!string)) {
        return 0;
    }
    int i = 0;
    cJSON *c = object->child;
    while (c && cJSON_strcasecmp(c->string, string))
        i++, c = c->next;
    if (c)
        return cJSON_DetachItemFromArray(object, i);
    return 0;
}
void cJSON_DeleteItemFromObject(cJSON *object, const char *string) {
    cJSON_Delete(cJSON_DetachItemFromObject(object, string));
}

// Replace array/object items with new ones.
void cJSON_ReplaceItemInArray(cJSON *array, int which, cJSON *newitem) {
    if ((!array) || (!newitem)) {
        return;
    }
    cJSON *c = array->child;
    while (c && which > 0)
        c = c->next, which--;
    if (!c)
        return;
    newitem->next = c->next;
    newitem->prev = c->prev;
    if (newitem->next)
        newitem->next->prev = newitem;
    if (c == array->child)
        array->child = newitem;
    else
        newitem->prev->next = newitem;
    c->next = c->prev = 0;
    cJSON_Delete(c);
}

void cJSON_ReplaceItemInObject(cJSON *object, const char *string,
        cJSON *newitem) {
    if ((!object) || (!newitem) || (!string)) {
        return; /* can not add */
    }
    int i = 0;
    cJSON *c = object->child;
    while (c && cJSON_strcasecmp(c->string, string))
        i++, c = c->next;
    if (c) {
        newitem->string = cJSON_strdup(string);
        cJSON_ReplaceItemInArray(object, i, newitem);
    }
}

// Create basic types:
cJSON *cJSON_CreateNull() {
    cJSON *item = cJSON_New_Item();
    if (!item)
        return 0; /* memory fail */
    item->type = cJSON_NULL;
    return item;
}
cJSON *cJSON_CreateTrue() {
    cJSON *item = cJSON_New_Item();
    if (!item)
        return 0; /* memory fail */
    item->type = cJSON_True;
    return item;
}
cJSON *cJSON_CreateFalse() {
    cJSON *item = cJSON_New_Item();
    if (!item)
        return 0; /* memory fail */
    item->type = cJSON_False;
    return item;
}
cJSON *cJSON_CreateNumber(double num) {
    cJSON *item = cJSON_New_Item();
    if (!item)
        return 0; /* memory fail */
    item->type = cJSON_Number;
    item->valuedouble = num;
    item->valueint = (int) num;
    return item;
}
cJSON *cJSON_CreateString(const char *string) {
    cJSON *item = cJSON_New_Item();
    if (!item)
        return 0; /* memory fail */
    item->type = cJSON_String;
    item->valuestring = cJSON_strdup(string);
    return item;
}
cJSON *cJSON_CreateArray() {
    cJSON *item = cJSON_New_Item();
    if (!item)
        return 0; /* memory fail */
    item->type = cJSON_Array;
    return item;
}
cJSON *cJSON_CreateObject() {
    cJSON *item = cJSON_New_Item();
    if (!item)
        return 0; /* memory fail */
    item->type = cJSON_Object;
    return item;
}

// Create Arrays:
cJSON *cJSON_CreateIntArray(int *numbers, int count) {
    int i;
    cJSON *n = 0, *p = 0, *a = cJSON_CreateArray();
    if (!a)
        return 0; /* memory fail */
    for (i = 0; i < count; i++) {
        n = cJSON_CreateNumber(numbers[i]);
        if (!n) {
            cJSON_Delete(a);
            return 0; /* memory fail */
        }
        if (!i)
            a->child = n;
        else
            suffix_object(p, n);
        p = n;
    }
    return a;
}
cJSON *cJSON_CreateFloatArray(float *numbers, int count) {
    int i;
    cJSON *n = 0, *p = 0, *a = cJSON_CreateArray();
    if (!a)
        return 0; /* memory fail */
    for (i = 0; i < count; i++) {
        n = cJSON_CreateNumber(numbers[i]);
        if (!n) {
            cJSON_Delete(a);
            return 0; /* memory fail */
        }
        if (!i)
            a->child = n;
        else
            suffix_object(p, n);
        p = n;
    }
    return a;
}
cJSON *cJSON_CreateDoubleArray(double *numbers, int count) {
    int i;
    cJSON *n = 0, *p = 0, *a = cJSON_CreateArray();
    if (!a)
        return 0; /* memory fail */
    for (i = 0; i < count; i++) {
        n = cJSON_CreateNumber(numbers[i]);
        if (!n) {
            cJSON_Delete(a);
            return 0; /* memory fail */
        }
        if (!i)
            a->child = n;
        else
            suffix_object(p, n);
        p = n;
    }
    return a;
}
cJSON *cJSON_CreateStringArray(const char **strings, int count) {
    int i;
    cJSON *n = 0, *p = 0, *a = cJSON_CreateArray();
    if (!a)
        return 0; /* memory fail */
    for (i = 0; i < count; i++) {
        n = cJSON_CreateString(strings[i]);
        if (!n) {
            cJSON_Delete(a);
            return 0; /* memory fail */
        }
        if (!i)
            a->child = n;
        else
            suffix_object(p, n);
        p = n;
    }
    return a;
}

/**************
 * UTIL calls for looking at values back
 **************/

/**
 * Validate cjson is bool value
 */
bool cJSON_is_val_bool(cJSON* cjson) {
    if (!cjson) {
        return false;
    }

    if ((cjson->type == cJSON_False) || (cjson->type == cJSON_True)) {
        return true;
    }

    return false;

}

/**
 * Validate cjson is number value
 */
bool cJSON_is_val_number(cJSON* cjson) {
    if (!cjson) {
        return false;
    }

    if (cjson->type == cJSON_Number) {
        return true;
    }

    return false;
}

/**
 * Validate cjson is string value
 */
bool cJSON_is_val_string(cJSON* cjson) {
    if (!cjson) {
        return false;
    }

    if ((cjson->type == cJSON_String) || (cjson->type == cJSON_Object_As_String) ){
        return true;
    }

    return false;
}

/**
 * Validate cjson is array value
 */
bool cJSON_is_val_array(cJSON* cjson) {
    if (!cjson) {
        return false;
    }

    if (cjson->type == cJSON_Array) {
        return true;
    }

    return false;
}

/**
 * Validate cjson is object value
 */
bool cJSON_is_val_object(cJSON* cjson) {
    if (!cjson) {
        return false;
    }

    if ((cjson->type == cJSON_Object)
            || (cjson->type == cJSON_Object_As_String)) {
        return true;
    }

    return false;
}

bool cJSON_get_bool_val(cJSON* cjson) {
    if (!cJSON_is_val_bool(cjson)) {
        return false;
    }

    if (cjson->type == cJSON_True) {
        return true;
    }

    return false;

}

int cJSON_get_int_val(cJSON* cjson) {
    if (!cJSON_is_val_number(cjson)) {
        return 0;
    }

    return cjson->valueint;
}

double cJSON_get_double_val(cJSON* cjson) {
    if (!cJSON_is_val_number(cjson)) {
        return 0;
    }

    return cjson->valuedouble;

}

char* cJSON_get_string_val(cJSON* cjson) {
    if (!cJSON_is_val_string(cjson)) {
        return NULL;
    }

    return cjson->valuestring;

}

void cJSON_Minify(char *json)
{
    char *into=json;
    while (*json)
    {
        if (*json==' ') json++;
        else if (*json=='\t') json++;    /* Whitespace characters. */
        else if (*json=='\r') json++;
        else if (*json=='\n') json++;
        else if (*json=='/' && json[1]=='/')  while (*json && *json!='\n') json++;   /* double-slash comments, to end of line. */
        else if (*json=='/' && json[1]=='*') {while (*json && !(*json=='*' && json[1]=='/')) json++;json+=2;}    /* multiline comments. */
        else if (*json=='\"'){*into++=*json++;while (*json && *json!='\"'){if (*json=='\\') *into++=*json++;*into++=*json++;}*into++=*json++;} /* string literals, which are \" sensitive. */
        else *into++=*json++;    /* All other characters. */
    }
    *into=0;     /* and null-terminate. */
}
