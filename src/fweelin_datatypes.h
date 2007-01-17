#ifndef __FWEELIN_DATATYPES_H
#define __FWEELIN_DATATYPES_H

enum CoreDataType {
  T_char,
  T_int,
  T_long,
  T_float,
  T_range,
  T_variable,
  T_variableref,
  T_invalid
};

CoreDataType GetCoreDataType(char *name);

class Range {
 public:
  Range(int lo, int hi) : lo(lo), hi(hi) {};

  int lo,
    hi;
};

// Flexible data type configuration variable-
// Used in parsing and evaluating expressions from config file

#define CFG_VAR_SIZE 16 // Number of data bytes in one variable
class UserVariable {

 public:
  UserVariable() : name(0), value(data), next(0) {};
  ~UserVariable() { 
    if (name != 0) 
      delete[] name;
  };

  // Ensures that the precision of this variable is at least that of src
  // If not, reconfigures this variable to match src..
  // For ex, if this is T_char and src is T_float, this becomes T_float
  void RaisePrecision (UserVariable &src) {
    switch (src.type) {
    case T_char :
      break;
    case T_int :
      if (type == T_char) {
	int tmp = (int) *this;
	type = T_int;
	*this = tmp;
      }
      break;
    case T_long :
      if (type == T_char || type == T_int) {
	long tmp = (long) *this;
	type = T_long;
	*this = tmp;
      }
      break;
    case T_float : 
      if (type == T_char || type == T_int || type == T_long) {
	float tmp = (float) *this;
	type = T_float;
	*this = tmp;
      }
      break;
    default :
      break;
    }
  };

  char operator > (UserVariable &cmp) {
    RaisePrecision(cmp);
    // Comparing ranges yields undefined results
    if (type == T_range || cmp.type == T_range)
      return 0;
    switch (type) {
      case T_char : 
	return (*((char *) value) > (char) cmp);
      case T_int : 
	return (*((int *) value) > (int) cmp);
      case T_long : 
	return (*((long *) value) > (long) cmp);
      case T_float : 
	return (*((float *) value) > (float) cmp);
      case T_variable :
      case T_variableref :
	printf(" UserVariable: WARNING: Compare T_variable or T_variableref "
	       " not implemented!\n");
	return 0;      
      case T_invalid : 
	printf(" UserVariable: WARNING: Can't compare invalid variable!\n");
	return 0;
    }
    
    return 0;
  };
  
  char operator == (UserVariable &cmp) {
    RaisePrecision(cmp);
    // Special case if one variable is range and one is scalar-- then
    // we check if the scalar is within the range
    if (type == T_range && cmp.type != T_range) {
      int v = (int) cmp;
      Range r(*((int *) value),*(((int *) value)+1));
      return (v >= r.lo && v <= r.hi);
    }
    if (cmp.type == T_range && type != T_range) {
      int v = (int) *this;
      Range r(*((int *) cmp.value),*(((int *) cmp.value)+1));
      return (v >= r.lo && v <= r.hi);
    }
    switch (type) {
    case T_char : 
      return (*((char *) value) == (char) cmp);
    case T_int : 
      return (*((int *) value) == (int) cmp);
    case T_long : 
      return (*((long *) value) == (long) cmp);
    case T_float : 
      return (*((float *) value) == (float) cmp);
    case T_range : 
      {
	Range r = (Range) cmp;
	return (*((int *) value) == r.lo && 
		*(((int *) value)+1) == r.hi);
      }
    case T_variable :
    case T_variableref :
      printf(" UserVariable: WARNING: Compare T_variable or T_variableref "
	     " not implemented!\n");
      return 0;      
    case T_invalid : 
      printf(" UserVariable: WARNING: Can't compare invalid variable!\n");
      return 0;
    }

    return 0;
  };

  char operator != (UserVariable &cmp) {
    return !(operator == (cmp));
  };

  void operator += (UserVariable &src) { 
    RaisePrecision(src);
    switch (type) {
    case T_char : 
      *((char *) value) += (char) src;
      break;
    case T_int : 
      *((int *) value) += (int) src;
      break;
    case T_long : 
      *((long *) value) += (long) src;
      break;
    case T_float : 
      *((float *) value) += (float) src;
      break;
    case T_range : 
      {
	Range r = (Range) src;
	*((int *) value) += r.lo; 
	*(((int *) value)+1) += r.hi;
      }
      break;
    case T_variable :
    case T_variableref :
      printf(" UserVariable: WARNING: Algebra on T_variable or T_variableref "
	     " not possible!\n");
      break;
    case T_invalid : 
      printf(" UserVariable: WARNING: Can't operate on invalid variable!\n");
      break;
    }
  };

  // Return the absolute value of the difference (delta) between this variable and arg
  UserVariable GetDelta (UserVariable &arg) {
    UserVariable ret;
    ret.type = T_char;
    ret.RaisePrecision(*this);
    ret.RaisePrecision(arg);
    
    switch (ret.type) {
      case T_char :
	ret = (char) abs((char) arg - (char) *this);
	break;
      case T_int :
	ret = (int) abs((int) arg - (int) *this);
	break;
      case T_long :
	ret = (long) labs((long) arg - (long) *this);
	break;
      case T_float :
	ret = (float) fabsf((float) arg - (float) *this);
	break;
      default :
	printf(" UserVariable: WARNING: GetDelta() doesn't work on this type of variable!\n");
	break;
    }
    
    return ret;
  };
  
  void operator -= (UserVariable &src) { 
    RaisePrecision(src);
    switch (type) {
    case T_char : 
      *((char *) value) -= (char) src;
      break;
    case T_int : 
      *((int *) value) -= (int) src;
      break;
    case T_long : 
      *((long *) value) -= (long) src;
      break;
    case T_float : 
      *((float *) value) -= (float) src;
      break;
    case T_range : 
      {
	Range r = (Range) src;
	*((int *) value) -= r.lo; 
	*(((int *) value)+1) -= r.hi;
      }
      break;
    case T_variable :
    case T_variableref :
      printf(" UserVariable: WARNING: Algebra on T_variable or T_variableref "
	     " not possible!\n");
      break;
    case T_invalid : 
      printf(" UserVariable: WARNING: Can't operate on invalid variable!\n");
      break;
    }
  };

  void operator *= (UserVariable &src) { 
    RaisePrecision(src);
    switch (type) {
    case T_char : 
      *((char *) value) *= (char) src;
      break;
    case T_int : 
      *((int *) value) *= (int) src;
      break;
    case T_long : 
      *((long *) value) *= (long) src;
      break;
    case T_float : 
      *((float *) value) *= (float) src;
      break;
    case T_range : 
      {
	Range r = (Range) src;
	*((int *) value) *= r.lo; 
	*(((int *) value)+1) *= r.hi;
      }
      break;
    case T_variable :
    case T_variableref :
      printf(" UserVariable: WARNING: Algebra on T_variable or T_variableref "
	     " not possible!\n");
      break;
    case T_invalid : 
      printf(" UserVariable: WARNING: Can't operate on invalid variable!\n");
      break;
    }
  };

  void operator /= (UserVariable &src) { 
    switch (type) {
    case T_char : 
    case T_int :
    case T_long :
    case T_float :
      {
	// Special case- when dividing a scalar by another scalar, the 
	// result is always evaluated to a float!!
	float t = (float) src;
	
	// Convert this variable to a float
	if (t != 0) {
	  *((float *) value) = (float) *this / t;
	  type = T_float;
	}
      }
      break;
    case T_range : 
      {
	Range r = (Range) src;
	if (r.lo != 0)
	  *((int *) value) /= r.lo; 
	if (r.hi != 0)
	  *(((int *) value)+1) /= r.hi;
      }
      break;
    case T_variable :
    case T_variableref :
      printf(" UserVariable: WARNING: Algebra on T_variable or T_variableref "
	     " not possible!\n");
      break;
    case T_invalid : 
      printf(" UserVariable: WARNING: Can't operate on invalid variable!\n");
      break;
    }
  };

  UserVariable & operator = (char src) { 
    *((char *) value) = src; 
    return *this;
  };
  UserVariable & operator = (int src) { 
    *((int *) value) = src; 
    return *this;
  };
  UserVariable & operator = (long src) { 
    *((long *) value) = src; 
    return *this;
  };
  UserVariable & operator = (float src) { 
    *((float *) value) = src; 
    return *this;
  };
  UserVariable & operator = (Range src) { 
    *((int *) value) = src.lo; 
    *(((int *) value)+1) = src.hi;
    return *this;
  };

  operator char () {
    switch (type) {
    case T_char : return *((char *) value);
    case T_int : return (char) *((int *) value);
    case T_long : return (char) *((long *) value);
    case T_float : return (char) *((float *) value);
    case T_range : 
      printf(" UserVariable: WARNING: Can't convert range to scalar!\n");
      return 0;
    case T_variable :
    case T_variableref :
      printf(" UserVariable: WARNING: Can't convert T_variable or "
	     "T_variableref!\n");
      return 0;      
    case T_invalid : 
      printf(" UserVariable: WARNING: Can't convert invalid variable!\n");
      return 0;
    }

    return 0;
  };
  operator int () {
    switch (type) {
    case T_char : return (int) *((char *) value);
    case T_int : return *((int *) value);
    case T_long : return (int) *((long *) value);
    case T_float : return (int) *((float *) value);
    case T_range : 
      printf(" UserVariable: WARNING: Can't convert range to scalar!\n");
      return 0;
    case T_variable :
    case T_variableref :
      printf(" UserVariable: WARNING: Can't convert T_variable or "
	     "T_variableref!\n");
      return 0;      
    case T_invalid : 
      printf(" UserVariable: WARNING: Can't convert invalid variable!\n");
      return 0;
    }

    return 0;
  };
  operator long () {
    switch (type) {
    case T_char : return (long) *((char *) value);
    case T_int : return (long) *((int *) value);
    case T_long : return *((long *) value);
    case T_float : return (long) *((float *) value);
    case T_range : 
      printf(" UserVariable: WARNING: Can't convert range to scalar!\n");
      return 0;
    case T_variable :
    case T_variableref :
      printf(" UserVariable: WARNING: Can't convert T_variable or "
	     "T_variableref!\n");
      return 0;      
    case T_invalid : 
      printf(" UserVariable: WARNING: Can't convert invalid variable!\n");
      return 0;
    }

    return 0;
  };
  operator float () {
    switch (type) {
    case T_char : return (float) *((char *) value);
    case T_int : return (float) *((int *) value);
    case T_long : return (float) *((long *) value);
    case T_float : return *((float *) value);
    case T_range : 
      printf(" UserVariable: WARNING: Can't convert range to scalar!\n");
      return 0;
    case T_variable :
    case T_variableref :
      printf(" UserVariable: WARNING: Can't convert T_variable or "
	     "T_variableref!\n");
      return 0;      
    case T_invalid : 
      printf(" UserVariable: WARNING: Can't convert invalid variable!\n");
      return 0;
    }

    return 0;
  };
  operator Range () {
    switch (type) {
    case T_char : return Range(*((char *) value), *((char *) value));
    case T_int : return Range(*((int *) value), *((int *) value));
    case T_long : return Range(*((long *) value), *((long *) value));
    case T_float : return Range((int) *((float *) value), 
				(int) *((float *) value));
    case T_range : return Range(*((int *) value),*(((int *) value)+1));
    case T_variable :
    case T_variableref :
      printf(" UserVariable: WARNING: Can't convert T_variable or "
	     "T_variableref!\n");
      return Range(0,0);      
    case T_invalid : 
      printf(" UserVariable: WARNING: Can't convert invalid variable!\n");
      return Range(0,0);
    }

    return Range(0,0);
  };

  void operator = (UserVariable &src) {
    // Assignment operator does not copy name to avoid memory alloc 
    // problems
    type = src.type;
    memcpy(data,src.data,CFG_VAR_SIZE);
    if (src.value == src.data) 
      value = data;
    else
      value = src.value; // System variable, copy data ptr directly
  };

  // Sets this UserVariable from src, converting from src type to this
  void SetFrom(UserVariable &src) {
    switch (type) {
    case T_char :
      *this = (char) src;
      break;
    case T_int : 
      *this = (int) src;
      break;
    case T_long : 
      *this = (long) src;
      break;
    case T_float :
      *this = (float) src;
      break;
    case T_range :
      *this = (Range) src;
      break;
    case T_variable :
    case T_variableref :
    case T_invalid : 
      printf(" UserVariable: WARNING: Can't set from invalid variable!\n");
      break;
    }
  }

  // Dump UserVariable to string str (maxlen is maximum length) 
  // or stdout if str = 0
  void Print(char *str = 0, int maxlen = 0);

  inline char IsSystemVariable() { return (value != data); };
  inline char *GetValue() { return value; }; // Returns the raw data bytes for the value of this variable
  inline CoreDataType GetType() { return type; };
  inline char *GetName() { return name; };
  
  char *name;
  CoreDataType type;
  char data[CFG_VAR_SIZE];

  // System variables are a special type of variable that is created by the 
  // core FreeWheeling system and not the user.
  //
  // A system variable is essentially a pointer to an internal data value
  // inside FreeWheeling. It can be read by the configuration system like
  // any other user variable, but it accesses directly into FreeWheeling's
  // internal memory and so it has a mind of its own
  //
  // If value points to the data array, this is a user variable
  // If value does not point to data, this is a system variable
  char *value;

  UserVariable *next;
};

#endif
