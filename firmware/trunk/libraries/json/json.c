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

#include "json.h"
#include "string.h"
#include "stdio.h"
#include <stdlib.h>
#include <ctype.h>

/** \defgroup json JSON
	The Make Controller JSON library provides a very small and very fast library for parsing and 
  generating json. 
  
  From http://www.json.org: "JSON (JavaScript Object Notation) 
  is a lightweight data-interchange format. It is easy for humans to read and write. It is easy for 
  machines to parse and generate."

  JSON is quite widely used when communicating with web servers, or other network enabled devices.
  It's nice and small, and easy to work with.  It's quite well supported in many programming
  environments, so it's not a bad option for a communication format when you need to talk to other
  devices from the Make Controller.  
  
  \b Disclaimer - in an attempt to keep it as small and as simple as possible, this library is not 
  completely full featured at the moment.  It doesn't deal with escaped strings or some of the 
  more exotic numeric representations outlined in the JSON specification.  It does, however, work
  quite well for most other JSON tasks.

  \b Generating
  Generating JSON is pretty simple - just make successive calls to the API to add the desired
  elements to your string.  
  
  You need to provide a few things:
   - a JsonEncode_State variable
   - the buffer in which to store the JSON string being built
   - a count of how many bytes are left in that buffer  
   The API will update that count, so it's not too much trouble.

  \code
  #define MAX_JSON_LEN 256
  char jsonbuf[MAX_JSON_LEN];
  int remaining = MAX_JSON_LEN;
  JsonEncode_State s;

  char *p = jsonbuf; // keep a pointer to the current location
  JsonEncode_Init(&s); // initialize our state variable
  p = JsonEncode_ObjectOpen(&s, p, &remaining);
  p = JsonEncode_String(&s, p, "hello", &remaining);
  p = JsonEncode_Int(&s, p, 234, &remaining);
  p = JsonEncode_ObjectClose(&s, p, &remaining);
  // now the string in jsonbuf looks like {"hello":234} - beautifully formatted JSON
  int json_len = MAX_JSON_LEN - remaining; // this is the total length of the string in jsonbuf
  \endcode

  \b Note - the library will add the appropriate separators (: or , usually) to the string, 
  depending on the context of the objects and arrays you've opened, or other data you've inserted.

  \b Parsing
  Parsing is done using an event-based mechanism.  This means you can register for any parse events you care
  to hear about, and then be called back with their value as they're encountered in the JSON string.
  Each parse process needs its own JsonDecode_State variable to keep track of where it is.

  In each callback, return true to continue parsing, or return false and parsing will stop.  

  If you need to pass around some context that you would like available in each of the callbacks, 
  you can pass it to JsonDecode_Init() and it will be passed to each of the callbacks you've registered.
  Otherwise, just pass 0 if you don't need it.

  \code
  // first, define the functions that we want to be called back on
  bool on_obj_opened(void* ctx)
  {
    // will be called when an object has been opened...
    return true; // keep parsing
  }
  
  bool on_int(void *ctx, int val)
  {
    iny my_json_int = val;
    // called when an int is encountered...
    return true; // keep parsing
  }
  
  bool on_string(void *ctx, char *string, int len)
  {
    // called when a string is encountered...
    return true; // keep parsing
  }

  // Now, register these callbacks with the JSON parser.
  JsonDecode_SetStartObjCallback(on_obj_opened);
  JsonDecode_SetIntCallback(on_int);
  JsonDecode_SetStringCallback(on_string);

  // Finally, run the parser.
  JsonDecode_State s;
  JsonDecode_Init(&s, 0); // pass 0 if you don't need to use any special context
  char jsonstr[] = "[{\"label\":\"value\",\"label2\":{\"nested\":234}}]";
  JsonDecode(&s, jsonstr, strlen(jsonstr));
  // now each of our callbacks will be triggered at the appropriate times
  \endcode

  Thanks to YAJL (http://code.google.com/p/yajl-c) for some design inspiration.
  \par

	\ingroup Libraries
	@{
*/

static void JsonEncode_AppendedAtom(JsonEncode_State* state);

/**
  Initialize or reset the state of a JsonEncode_State variable.
  Be sure to do this each time before you start parsing.
*/
void JsonEncode_Init(JsonEncode_State* state)
{
  state->depth = 0;
  state->steps[0] = JSON_START;
}

/**
  Open up a new JSON object.
  This adds an opening '{' to the json string.
  @param state A pointer to the JsonEncode_State variable being used for this encode process.
  @param buf A pointer to the buffer holding the JSON string.
  @param remaining A pointer to the count of how many bytes are left in your JSON buffer.
  @return A pointer to the location in the JSON buffer after this element has been added, or NULL if there was no room.
*/
char* JsonEncode_ObjectOpen(JsonEncode_State* state, char *buf, int *remaining)
{
  int len = 1;
  switch(state->steps[state->depth])
  {
    case JSON_ARRAY_START:
    case JSON_OBJ_START:
    case JSON_START:
    {
      if(*remaining < len)
        return NULL;
      memcpy(buf, "{", len);
      break;
    }
    case JSON_OBJ_KEY:
    case JSON_IN_ARRAY:
    {
      len += 1; // for ,
      if(*remaining < len)
        return NULL;
      memcpy(buf, ",{", len);
      break;
    }
    case JSON_OBJ_VALUE:
    {
      len += 1; // for :
      if(*remaining < len)
        return NULL;
      memcpy(buf, ":{", len);
      break;
    }
  }
  
  if(++state->depth > JSON_MAX_DEPTH)
    return NULL;
  state->steps[state->depth] = JSON_OBJ_START;
  (*remaining) -= len;
  return buf + len;
}

/**
  Set the key for a JSON object.
  This is a convenience function that simply calls JsonEncode_String().
  It is provided to help enforce the idea that the first member of a JSON
  object pair must be a string.
  @see JsonEncode_String()
*/
char* JsonEncode_ObjectKey(JsonEncode_State* state, char *buf, const char *key, int *remaining)
{
  return JsonEncode_String(state, buf, key, remaining);
}

/**
  Close a JSON object.
  Adds a closing '}' to the string.
  @param state A pointer to the JsonEncode_State variable being used for this encode process.
  @param buf A pointer to the buffer which contains your JSON string.
  @param remaining A pointer to an integer keeping track of how many bytes are left in your JSON buffer.
  @return A pointer to the location in the JSON buffer after this element has been added, or NULL if there was no room.
*/
char* JsonEncode_ObjectClose(JsonEncode_State* state, char *buf, int *remaining)
{
  if(*remaining < 1)
    return NULL;
  memcpy(buf, "}", 1);
  state->depth--;
  JsonEncode_AppendedAtom(state);
  (*remaining)--;
  return buf + 1;
}

/**
  Open up a new JSON array.
  This adds an opening '[' to the json string.
  @param state A pointer to the JsonEncode_State variable being used for this encode process.
  @param buf A pointer to the buffer holding the JSON string.
  @param remaining A pointer to the count of how many bytes are left in your JSON buffer.
  @return A pointer to the location in the JSON buffer after this element has been added, or NULL if there was no room.
*/
char* JsonEncode_ArrayOpen(JsonEncode_State* state, char *buf, int *remaining)
{
  int len = 1;
  switch(state->steps[state->depth])
  {
    case JSON_ARRAY_START:
    case JSON_OBJ_START:
    case JSON_START:
    {
      if(*remaining < len)
        return NULL;
      memcpy(buf, "[", len);
      break;
    }
    case JSON_OBJ_KEY:
    case JSON_IN_ARRAY:
    {
      len += 1; // for ,
      if(*remaining < len)
        return NULL;
      memcpy(buf, ",[", len);
      break;
    }
    case JSON_OBJ_VALUE:
    {
      len += 1; // for :
      if(*remaining < len)
        return NULL;
      memcpy(buf, ":[", len);
      break;
    }
  }
  if(++state->depth > JSON_MAX_DEPTH)
    return NULL;
  state->steps[state->depth] = JSON_ARRAY_START;
  (*remaining) -= len;
  return buf + len;
}

/**
  Close an array.
  Adds a closing ']' to the string.
  @param state A pointer to the JsonEncode_State variable being used for this encode process.
  @param buf A pointer to the buffer which contains your JSON string.
  @param remaining A pointer to the count of how many bytes are left in your JSON buffer.
  @return A pointer to the JSON buffer after this element has been added, or NULL if there was no room.
*/
char* JsonEncode_ArrayClose(JsonEncode_State* state, char *buf, int *remaining)
{
  if(*remaining < 1)
    return NULL;
  memcpy(buf, "]", 1);
  state->depth--;
  JsonEncode_AppendedAtom(state);
  (*remaining)--;
  return buf + 1;
}

/**
  Add a string to the current JSON string.
  Depending on whether you've opened objects, arrays, or other inserted 
  other data, the approprate separating symbols will be added to the string.

  Doesn't handle escaped elements in the string at the moment, since the internal 
  implementation simply points to the string in the original data.

  @param state A pointer to the JsonEncode_State variable being used for this encode process.
  @param buf A pointer to the buffer containing the JSON string.
  @param string The string to be added.
  @param remaining A pointer to the count of bytes remaining in the JSON buffer.
  @return A pointer to the JSON buffer after this element has been added, or NULL if there was no room.
*/
char* JsonEncode_String(JsonEncode_State* state, char *buf, const char *string, int *remaining)
{
  int string_len = strlen(string) + 2 /* quotes */;

  switch(state->steps[state->depth])
  {
    case JSON_ARRAY_START:
    case JSON_OBJ_START:
    {
      if(*remaining < string_len)
        return NULL;
      char temp[string_len+1];
      snprintf(temp, string_len+1, "\"%s\"", string);
      memcpy(buf, temp, string_len);
      break;
    }
    case JSON_OBJ_KEY:
    case JSON_IN_ARRAY:
    {
      string_len += 1; // for ,
      if(*remaining < string_len)
        return NULL;
      char temp[string_len+1];
      snprintf(temp, string_len+1, ",\"%s\"", string);
      memcpy(buf, temp, string_len);
      break;
    }
    case JSON_OBJ_VALUE:
    {
      string_len += 1; // for :
      if(*remaining < string_len)
        return NULL;
      char temp[string_len+1];
      snprintf(temp, string_len+1, ":\"%s\"", string);
      memcpy(buf, temp, string_len);
      break;
    }
    default:
      return NULL;
  }
  JsonEncode_AppendedAtom(state);
  (*remaining) -= string_len;
  return buf + string_len;
}

/**
  Add an int to a JSON string.

  @param state A pointer to the JsonEncode_State variable being used for this encode process.
  @param buf A pointer to the buffer containing the JSON string.
  @param value The integer to be added.
  @param remaining A pointer to the count of bytes remaining in the JSON buffer.
  @return A pointer to the JSON buffer after this element has been added, or NULL if there was no room.
*/
char* JsonEncode_Int(JsonEncode_State* state, char *buf, int value, int *remaining)
{
  int int_len = 4;
  switch(state->steps[state->depth])
  {
    case JSON_ARRAY_START:
    {
      if(*remaining < int_len)
        return NULL;
      char temp[int_len+1];
      snprintf(temp, int_len+1, "%d", value);
      memcpy(buf, temp, int_len);
      int_len = strlen(temp);
      break;
    }
    case JSON_IN_ARRAY:
    {
      int_len += 1; // for ,
      if(*remaining < int_len)
        return NULL;
      char temp[int_len+1];
      snprintf(temp, int_len+1, ",%d", value);
      memcpy(buf, temp, int_len);
      int_len = strlen(temp);
      break;
    }
    case JSON_OBJ_VALUE:
    {
      int_len += 1; // for :
      if(*remaining < int_len)
        return NULL;
      char temp[int_len+1];
      snprintf(temp, int_len+1, ":%d", value);
      int_len = strlen(temp);
      memcpy(buf, temp, int_len);
      break;
    }
    default:
      return NULL; // bogus state
  }
  JsonEncode_AppendedAtom(state);
  (*remaining) -= int_len;
  return buf + int_len;
}

/**
  Add a boolean value to a JSON string.

  @param state A pointer to the JsonEncode_State variable being used for this encode process.
  @param buf A pointer to the buffer containing the JSON string.
  @param value The boolean value to be added.
  @param remaining A pointer to the count of bytes remaining in the JSON buffer.
  @return A pointer to the JSON buffer after this element has been added, or NULL if there was no room.
*/
char* JsonEncode_Bool(JsonEncode_State* state, char *buf, bool value, int *remaining)
{
  const char* boolval = (value) ? "true" : "false";
  int bool_len = strlen(boolval);
  switch(state->steps[state->depth])
  {
    case JSON_ARRAY_START:
    {
      if(*remaining < bool_len)
        return NULL;
      char temp[bool_len+1];
      snprintf(temp, bool_len+1, "%s", boolval);
      memcpy(buf, temp, bool_len);
      break;
    }
    case JSON_IN_ARRAY:
    {
      bool_len += 1; // for ,
      if(*remaining < bool_len)
        return NULL;
      char temp[bool_len+1];
      snprintf(temp, bool_len+1, ",%s", boolval);
      memcpy(buf, temp, bool_len);
      break;
    }
    case JSON_OBJ_VALUE:
    {
      bool_len += 1; // for :
      if(*remaining < bool_len)
        return NULL;
      char temp[bool_len+1];
      snprintf(temp, bool_len+1, ":%s", boolval);
      memcpy(buf, temp, bool_len);
      break;
    }
    default:
      return NULL; // bogus state
  }
  JsonEncode_AppendedAtom(state);
  (*remaining) -= bool_len;
  return buf + bool_len;
}

/*
  Called after adding a new value (atom) to a string in order
  to update the state appropriately.
*/
// static
void JsonEncode_AppendedAtom(JsonEncode_State* state)
{
  switch(state->steps[state->depth])
  {
    case JSON_OBJ_START:
    case JSON_OBJ_KEY:
      state->steps[state->depth] = JSON_OBJ_VALUE;
      break;
    case JSON_ARRAY_START:
      state->steps[state->depth] = JSON_IN_ARRAY;
      break;
    case JSON_OBJ_VALUE:
      state->steps[state->depth] = JSON_OBJ_KEY;
      break;
    default:
      break;
  }
}

/****************************************************************************
 JsonDecode
****************************************************************************/

typedef enum {
    token_true,
    token_false,
    token_colon,
    token_comma,     
    token_eof,
    token_error,
    token_left_brace,     
    token_left_bracket,
    token_null,         
    token_right_brace,     
    token_right_bracket,
    token_number, 
    token_maybe_negative,
    token_string,
    token_unknown
} JsonDecode_Token;

static JsonDecode_Token JsonDecode_GetToken(char* text, int len);

/**
  Set the function to be called back when an integer is parsed.
  The function must have the format:
  \code
  bool on_int(void* context, int value);
  \endcode
  which would then be registered like so:
  \code
  JsonDecode_State s;
  JsonDecode_SetIntCallback(&s, on_int);
  \endcode
  The \b context parameter can be optionally passed to JsonDecode_Init() 
  to provide context within the callback.

  @param state A pointer to the JsonDecode_State variable being used for this decode process.
  @param int_callback The function to be called back.
*/
void JsonDecode_SetIntCallback(JsonDecode_State* state, bool(*int_callback)(void *ctx, int val))
{
  state->int_callback = int_callback;
}

/**
  Set the function to be called back when a float is parsed.
  The function must have the format:
  \code
  bool on_float(void* context, float value);
  \endcode
  which would then be registered like so:
  \code
  JsonDecode_State s;
  JsonDecode_SetFloatCallback(&s, on_float);
  \endcode
  The \b context parameter can be optionally passed to JsonDecode_Init() 
  to provide context within the callback.

  @param state A pointer to the JsonDecode_State variable being used for this decode process.
  @param float_callback The function to be called back.
*/
void JsonDecode_SetFloatCallback(JsonDecode_State* state, bool(*float_callback)(void *ctx, float val))
{
  state->float_callback = float_callback;
}

/**
  Set the function to be called back when a boolean is parsed.
  The function must have the format:
  \code
  bool on_bool(void* context, bool value);
  \endcode
  which would then be registered like so:
  \code
  JsonDecode_State s;
  JsonDecode_SetBoolCallback(&s, on_bool);
  \endcode
  The \b context parameter can be optionally passed to JsonDecode_Init() 
  to provide context within the callback.

  @param state A pointer to the JsonDecode_State variable being used for this decode process.
  @param bool_callback The function to be called back.
*/
void JsonDecode_SetBoolCallback(JsonDecode_State* state, bool(*bool_callback)(void *ctx, bool val))
{
  state->bool_callback = bool_callback;
}

/**
  Set the function to be called back when a string is parsed.
  The function must have the format:
  \code
  bool on_string(void* context, char* string);
  \endcode
  which would then be registered like so:
  \code
  JsonDecode_State s;
  JsonDecode_SetStringCallback(&s, on_string);
  \endcode
  The \b context parameter can be optionally passed to JsonDecode_Init() 
  to provide context within the callback.

  @param state A pointer to the JsonDecode_State variable being used for this decode process.
  @param string_callback The function to be called back.
*/
void JsonDecode_SetStringCallback(JsonDecode_State* state, bool(*string_callback)(void *ctx, char *string, int len))
{
  state->string_callback = string_callback;
}

/**
  Set the function to be called back when an object is started.
  The left bracket - { - is the opening element of an object.
  The function must have the format:
  \code
  bool on_obj_started(void* context);
  \endcode
  which would then be registered like so:
  \code
  JsonDecode_State s;
  JsonDecode_SetStartObjCallback(&s, on_obj_started);
  \endcode
  The \b context parameter can be optionally passed to JsonDecode_Init() 
  to provide context within the callback.

  @param state A pointer to the JsonDecode_State variable being used for this decode process.
  @param start_obj_callback The function to be called back.
*/
void JsonDecode_SetStartObjCallback(JsonDecode_State* state, bool(*start_obj_callback)(void *ctx))
{
  state->start_obj_callback = start_obj_callback;
}

/**
  Set the function to be called back when the key of a key-value pair has been encountered.
  A key must always be a string in JSON, so you'll get the string back.  This is particularly
  helpful for setting how the next element (the value in the key-value pair) should be handled.
  The function must have the format:
  \code
  bool on_obj_key(void* context);
  \endcode
  which would then be registered like so:
  \code
  JsonDecode_State s;
  JsonDecode_SetObjKeyCallback(&s, on_obj_key);
  \endcode
  The \b context parameter can be optionally passed to JsonDecode_Init() 
  to provide context within the callback.

  @param state A pointer to the JsonDecode_State variable being used for this decode process.
  @param obj_key_callback The function to be called back.
*/
void JsonDecode_SetObjKeyCallback(JsonDecode_State* state, bool(*obj_key_callback)(void *ctx, char *key, int len))
{
  state->obj_key_callback = obj_key_callback;
}

/**
  Set the function to be called back when an object is ended.
  The right bracket - } - is the closing element of an object.
  The function must have the format:
  \code
  bool on_obj_ended(void* context);
  \endcode
  which would then be registered like so:
  \code
  JsonDecode_State s;
  JsonDecode_SetEndObjCallback(&s, on_obj_ended);
  \endcode
  The \b context parameter can be optionally passed to JsonDecode_Init() 
  to provide context within the callback.

  @param state A pointer to the JsonDecode_State variable being used for this decode process.
  @param end_obj_callback The function to be called back.
*/
void JsonDecode_SetEndObjCallback(JsonDecode_State* state, bool(*end_obj_callback)(void *ctx))
{
  state->end_obj_callback = end_obj_callback;
}

/**
  Set the function to be called back when an array is started.
  The left brace - [ - is the starting element of an array.
  The function must have the format:
  \code
  bool on_array_started(void* context);
  \endcode
  which would then be registered like so:
  \code
  JsonDecode_State s;
  JsonDecode_SetStartArrayCallback(&s, on_array_started);
  \endcode
  The \b context parameter can be optionally passed to JsonDecode_Init() 
  to provide context within the callback.

  @param state A pointer to the JsonDecode_State variable being used for this decode process.
  @param start_array_callback The function to be called back.
*/
void JsonDecode_SetStartArrayCallback(JsonDecode_State* state, bool(*start_array_callback)(void *ctx))
{
  state->start_array_callback = start_array_callback;
}

/**
  Set the function to be called back when an array is ended.
  The right brace - ] - is the closing element of an array.
  The function must have the format:
  \code
  bool on_array_ended(void* context);
  \endcode
  which would then be registered like so:
  \code
  JsonDecode_State s;
  JsonDecode_SetEndArrayCallback(&s, on_array_ended);
  \endcode
  The \b context parameter can be optionally passed to JsonDecode_Init() 
  to provide context within the callback.
  
  @param state A pointer to the JsonDecode_State variable being used for this decode process.
  @param end_array_callback The function to be called back.
*/
void JsonDecode_SetEndArrayCallback(JsonDecode_State* state, bool(*end_array_callback)(void *ctx))
{
  state->end_array_callback = end_array_callback;
}

/**
  Initialize or reset a JsonDecode_State variable.
  Do this prior to making a call to JsonDecode().
  @param state A pointer to the JsonDecode_State variable being used for this decode process.
  @param context An optional paramter that your code can use to 
  pass around a known object within the callbacks.  Otherwise, just set it to 0
*/
void JsonDecode_Init(JsonDecode_State* state, void* context)
{
  state->depth = 0;
  state->gotcomma = false;
  state->context = context;
  state->p = 0;
  state->len = 0;
}

/**
  Parse a JSON string.
  The JSON parser is event based, meaning that you will receive any callbacks
  you registered for as the elements are encountered in the JSON string.

  @param state A pointer to the JsonDecode_State variable being used for this decode process.
  @param text The JSON string to parse.
  @param len The length of the JSON string.
  @return True on a successful parse, false on failure.

  \par Example
  \code
  // quotes are escaped since I'm writing it out manually
  JsonDecode_State s;
  char jsonstr[] = "[{\"label\":\"value\",\"label2\":{\"nested\":234}}]";
  JsonDecode_Init(&s, 0);
  JsonDecode(jsonstr, strlen(jsonstr), 0); // don't pass in any context
  // now we expect to be called back on any callbacks we registered.
  \endcode
*/
bool JsonDecode(JsonDecode_State* state, char* text, int len)
{
  JsonDecode_Token token;
  
  // if these haven't been initialized, do it
  if(!state->p) 
    state->p = text;
  if(!state->len)
    state->len = len;

  while(state->len)
  {
    while(*state->p == ' ') // eat white space
      state->p++;
    token = JsonDecode_GetToken( state->p, state->len);
    switch(token)
    {
      case token_true:
        if(state->bool_callback)
        {
          if(!state->bool_callback(state->context, true))
            return false;
        }
        state->p += 4;
        state->len -= 4;
        break;
      case token_false:
        if(state->bool_callback)
        {
          if(!state->bool_callback(state->context, false))
            return false;
        }
        state->p += 5;
        state->len -= 5;
        break;
      case token_null:
        if(state->null_callback)
        {
          if(!state->null_callback(state->context))
            return false;
        }
        state->p += 4;
        state->len -= 4;
        break;
      case token_comma:
        state->gotcomma = true;
        // intentional fall-through
      case token_colon: // just get the next one      
        state->p++;
        state->len--;
        break;
      case token_left_bracket:
        state->steps[++state->depth] = JSON_DECODE_OBJECT_START;
        if(state->start_obj_callback)
        {
          if(!state->start_obj_callback(state->context))
            return false;
        }
        state->p++;
        state->len--;
        break;
      case token_right_bracket:
        state->depth--;
        if(state->end_obj_callback)
        {
          if(!state->end_obj_callback(state->context))
            return false;
        }
        state->p++;
        state->len--;
        break;
      case token_left_brace:
        state->steps[++state->depth] = JSON_DECODE_IN_ARRAY;
        if(state->start_array_callback)
        {
          if(!state->start_array_callback(state->context))
            return false;
        }
        state->p++;
        state->len--;
        break;
      case token_right_brace:
        state->depth--;
        if(state->end_array_callback)
        {
          if(!state->end_array_callback(state->context))
            return false;
        }
        state->p++;
        state->len--;
        break;
      case token_number:
      {
        const char* p = state->p;
        bool keepgoing = true;
        bool gotdecimal = false;
        do
        {
          if(*p == '.')
          {
            if(gotdecimal) // we only expect to get one decimal place in a number
              return false;
            gotdecimal = true;
            p++;
          }
          else if(!isdigit(*p))
            keepgoing = false;
          else
            p++;
        } while(keepgoing);
        int size = p - state->p;
        if(gotdecimal)
        {
          if(state->float_callback)
          {
            if(!state->float_callback(state->context, atof(state->p)))
              return false;
          }
        }
        else
        {
          if(state->int_callback)
          {
            if(!state->int_callback(state->context, atoi(state->p)))
              return false;
          }
        }
        state->p += size;
        state->len -= size;
        break;
      }
      case token_string:
      {
        char* p = ++state->p;
        state->len--;
        while(*p != '"') // just go until the next " for now...may ultimately check for escaped data
          p++;
        int size = p - state->p;
        *p = 0; // replace the trailing " with a null to make a string

        // figure out if this is a key or a normal string
        bool objkey = false;
        if(state->steps[state->depth] == JSON_DECODE_OBJECT_START)
        {
          state->steps[state->depth] = JSON_DECODE_IN_OBJECT;
          objkey = true;
        }
        if(state->gotcomma && state->steps[state->depth] == JSON_DECODE_IN_OBJECT)
        {
          state->gotcomma = false;
          objkey = true;
        }

        if(objkey) // last one was a comma - next string has to be a key
        {
          if(state->obj_key_callback)
          {
            if(!state->obj_key_callback(state->context, state->p, size))
              return false;
          }
        }
        else // just a normal string
        {
          if(state->string_callback)
          {
            if(!state->string_callback(state->context, state->p, size))
              return false;
          }
        }
        state->p += (size+1); // account for the trailing "
        state->len -= (size+1);
        break;
      }
      case token_eof: // we don't expect to get this, since our len should run out before we get eof
        return false;
      default:
        return false;
    }
  }
  return true;
}

/** @}
*/

// static
JsonDecode_Token JsonDecode_GetToken(char* text, int len)
{
  char c = text[0];
  switch(c)
  {
    case ':':
      return token_colon;
    case ',':
      return token_comma;
    case '{':
      return token_left_bracket;
    case '}':
      return token_right_bracket;
    case '[':
      return token_left_brace;
    case ']':
      return token_right_brace;
    case '"':
      return token_string;
    case '0': case '1': case '2': case '3': case '4': 
    case '5': case '6': case '7': case '8': case '9':
      return token_number;
    case '-':
      return token_maybe_negative;
    case 't':
    {
      if(len < 4) // not enough space;
        return token_unknown;
      if(!strncmp(text, "true", 4))
        return token_true;
      else 
        return token_unknown;
    }
    case 'f':
    {
      if(len < 5) // not enough space;
        return token_unknown;
      if(!strncmp(text, "false", 5))
        return token_false;
      else
        return token_unknown;
    }
    case 'n':
    {
      if(len < 4) // not enough space;
        return token_unknown;
      if(!strncmp(text, "null", 4))
        return token_null;
      else
        return token_unknown;
    }
    case '\0':
      return token_eof;
    default:
      return token_unknown;
  }
}





