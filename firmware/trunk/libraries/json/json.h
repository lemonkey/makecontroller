/*********************************************************************************

 Copyright 2006-2008 MakingThings

 Licensed under the Apache License, 
 Version 2.0 (the "License"); you may not use this file except in compliance 
 with the License. You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0 
 
 Unless required by applicable law or agreed to in writing, software distributed
 under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 CONDITIONS OF ANY KIND, either express or implied. See the License for
 the specific language governing permissions and limitations under the License.

*********************************************************************************/

#ifndef JSON_H
#define JSON_H

#include "types.h"

#define JSON_MAX_DEPTH 32

// state object for encoding
typedef enum
{
  JSON_START,
  JSON_OBJ_START,
  JSON_OBJ_KEY,
  JSON_OBJ_VALUE,
  JSON_ARRAY_START,
  JSON_IN_ARRAY
} JsonEncode_Step;

/**
  The structure used to maintain the state of a JSON encode process.
  You'll need to have one of these for each JSON string you want to encode.
  The same variable can be reused after resetting it with a call to JsonEncode_Init().
  \ingroup json
 */
typedef struct
{
  JsonEncode_Step steps[JSON_MAX_DEPTH];
  int depth;
} JsonEncode_State;

// state object for decoding
typedef enum {
  JSON_DECODE_OBJECT_START,
  JSON_DECODE_IN_OBJECT,
  JSON_DECODE_IN_ARRAY
} JsonDecode_Step;

/**
  The structure used to maintain the state of a JSON decode process.
  You'll need to have one of these for each JSON string you want to encode.
  The same variable can be reused after resetting it with a call to JsonDecode_Init().
  \ingroup json
 */
typedef struct {
  JsonDecode_Step steps[JSON_MAX_DEPTH];
  int depth;
  bool gotcomma;
  void* context;
  char* p;
  int len;
  bool(*null_callback)(void*);
  bool(*bool_callback)(void*, bool);
  bool(*int_callback)(void*, int);
  bool(*float_callback)(void*, float);
  bool(*string_callback)(void*, char*, int);
  bool(*start_obj_callback)(void*);
  bool(*obj_key_callback)(void*, char*, int);
  bool(*end_obj_callback)(void*);
  bool(*start_array_callback)(void*);
  bool(*end_array_callback)(void*);
} JsonDecode_State;

// Encode
void JsonEncode_Init(JsonEncode_State* state);

char* JsonEncode_ObjectOpen(JsonEncode_State* state, char *buf, int *remaining);
char* JsonEncode_ObjectKey(JsonEncode_State* state, char *buf, const char *key, int *remaining);
char* JsonEncode_ObjectClose(JsonEncode_State* state, char *buf, int *remaining);
char* JsonEncode_ArrayOpen(JsonEncode_State* state, char *buf, int *remaining);
char* JsonEncode_ArrayClose(JsonEncode_State* state, char *buf, int *remaining);
char* JsonEncode_String(JsonEncode_State* state, char *buf, const char *string, int *remaining);
char* JsonEncode_Int(JsonEncode_State* state, char *buf, int value, int *remaining);
char* JsonEncode_Bool(JsonEncode_State* state, char *buf, bool value, int *remaining);

// Decode
void JsonDecode_SetIntCallback(JsonDecode_State* state, bool(*int_callback)(void *ctx, int val));
void JsonDecode_SetFloatCallback(JsonDecode_State* state, bool(*float_callback)(void *ctx, float val));
void JsonDecode_SetBoolCallback(JsonDecode_State* state, bool(*bool_callback)(void *ctx, bool val));
void JsonDecode_SetStringCallback(JsonDecode_State* state, bool(*string_callback)(void *ctx, char *string, int len));
void JsonDecode_SetStartObjCallback(JsonDecode_State* state, bool(*start_obj_callback)(void *ctx));
void JsonDecode_SetObjKeyCallback(JsonDecode_State* state, bool(*obj_key_callback)(void *ctx, char *key, int len));
void JsonDecode_SetEndObjCallback(JsonDecode_State* state, bool(*end_obj_callback)(void *ctx));
void JsonDecode_SetStartArrayCallback(JsonDecode_State* state, bool(*start_array_callback)(void *ctx));
void JsonDecode_SetEndArrayCallback(JsonDecode_State* state, bool(*end_array_callback)(void *ctx));

void JsonDecode_Init(JsonDecode_State* state, void* context);
bool JsonDecode(JsonDecode_State* state, char* text, int len);

#endif /* JSON_H */


