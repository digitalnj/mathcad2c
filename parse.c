#include <stdio.h>
#include <string.h>
#include <stdlib.h>
//#include <dmalloc.h>

#define PPREFIX	"++"

#define DEBUGLEVEL	2

#define PARSE_MAX_FUNC_ARGS	128

#define dprintf(lev, fmt, args...)   if(lev <= DEBUGLEVEL) printf(fmt, ## args)

char *handle_apply(FILE *fi, int level, int inif, int ineval);

char *get1stchar(char *istr) 
{
  int len= strlen(istr);
  int offset= strspn(istr, " \t\n\r");

  if (offset < len) {
    return(istr + offset);
  }

  return(NULL);
}

int get_id2(char *istr, char **first, char **second) {
  int len= strlen(istr);
  int soffset= strcspn(istr, ">") + 1;
  int eoffset;

  if (soffset >= len) return(-1);
  eoffset= strcspn(istr + soffset, "<");
  if (eoffset + soffset < len) {
    *(istr + eoffset + soffset)= 0;
  }

  if (!*first) {
    *first= strdup(istr + soffset);
    dprintf(1, PPREFIX"first: %s\n", *first);
    return(0);
  } else {
    *second= strdup(istr + soffset);
    dprintf(1, PPREFIX"second: %s\n", *second);
    return(1);
  }

  return(-1);
}

#define APPLY_UNKN    0
#define APPLY_INDEXER 1
#define APPLY_EQUAL   2
#define APPLY_PLUS    3
#define APPLY_MINUS   4
#define APPLY_MULT    5
#define APPLY_DIV     6
#define APPLY_POW     7
#define APPLY_NEG     8
#define APPLY_AND     9
#define APPLY_GEQ     10
#define APPLY_LEQ     11
#define APPLY_LT      12
#define APPLY_GT      13
#define APPLY_EVAL    14
#define APPLY_TANH    15

char *handle_return(FILE *fi, int level)
{
  char *istr, *buffer= malloc(1024);
  char *stret= NULL;
  char *idname= NULL;

  dprintf(1, PPREFIX"ml:parens(lev%d):\n", level);

  while (istr= fgets(buffer, 1024, fi)) {
    dprintf(2, "::%s", istr);
    if (istr= get1stchar(istr)) {
      if (!strncmp(istr, "<ml:id", 6)) {
	dprintf(1, PPREFIX"ml:id(name): %s\n", istr);
	get_id2(istr + 6, &idname, &idname);
	if (!stret) {
	  stret= malloc(2048);
	  *stret= 0;
	}
	sprintf(stret + strlen(stret), "return(%s);\n", idname);
	break;
      } 
    }
  }

  return(stret);
}

char *handle_parens(FILE *fi, int level)
{
  char *istr, *buffer= malloc(1024);
  char *stret= NULL;

  dprintf(1, PPREFIX"ml:parens(lev%d):\n", level);

  while (istr= fgets(buffer, 1024, fi)) {
    dprintf(2, "::%s", istr);
    if (istr= get1stchar(istr)) {
      if (!strncmp(istr, "<ml:apply", 9)) {
	char *stret2= handle_apply(fi, level + 1, 0, 0);
	dprintf(1, PPREFIX"ml:apply-f(lev%d): ret %s\n", level, stret2);
	if (!stret) {
	  stret= malloc(2048);
	  *stret= 0;
	}
	sprintf(stret + strlen(stret), "(");
	strcat(stret, stret2);
      } else if (!strncmp(istr, "</ml:parens", 11)) {
	dprintf(1, PPREFIX"ml:parens close\n");
	strcat(stret, ")");
	break;
      }
    }
  }

  return(stret);
}

char *handle_function(FILE *fi, int level) 
{
  char *istr, *buffer= malloc(1024);
  char *fname= NULL;
  char **fargs= (char **) calloc(PARSE_MAX_FUNC_ARGS, sizeof(char*));
  char *stret= NULL;
  int   bvars= 0, nargs= 0, i;

  dprintf(1, PPREFIX"ml:function(lev%d):\n", level);

  while (istr= fgets(buffer, 1024, fi)) {
    dprintf(2, "::%s", istr);
    if (istr= get1stchar(istr)) {
      if (!fname && !strncmp(istr, "<ml:id", 6)) {
	dprintf(1, PPREFIX"ml:id(name): %s\n", istr);
	get_id2(istr + 6, &fname, &fname);
	break;
      }
    }
  }

  if (!istr) {
    free(buffer);
    return(NULL);
  }

  while (istr= fgets(buffer, 1024, fi)) {
    dprintf(2, "::%s", istr);
    if (istr= get1stchar(istr)) {
      if (!strncmp(istr, "<ml:id", 6) && nargs < PARSE_MAX_FUNC_ARGS) {
	dprintf(1, PPREFIX"ml:id: %s\n", istr);
	if (bvars) {
	  if (get_id2(istr + 6, &fargs[nargs], NULL) == 0) nargs++;
	  else 
	    dprintf(1, PPREFIX"FAULTY boundVar!\n");
	} else get_id2(istr + 6, &fname, NULL);
      } else if (!strncmp(istr, "<ml:apply", 9)) {
	char *stret2= handle_apply(fi, level + 1, 0, 0);
	dprintf(1, PPREFIX"ml:apply-f(lev%d): ret %s\n", level, stret2);
	if (bvars) {
	  fargs[nargs++]= stret2;
	} else printf("NESTED F-APPLY ERROR\n");
      } else if (!strncmp(istr, "<ml:function", 12)) {
	char *stret2= handle_function(fi, level + 1);
	dprintf(1, PPREFIX"ml:function-f(lev%d): ret %s\n", level, stret2);
	if (bvars) {
	  fargs[nargs++]= stret2;
	} else printf("NESTED F-FUNCTION ERROR\n");
      } else if (!strncmp(istr, "<ml:boundVars", 13)) {
	dprintf(1, PPREFIX"ml:bVars:\n");
	bvars= 1;
      } else if (!strncmp(istr, "</ml:boundVars>", 11)) {
	if (fname) {
	  int slen= strlen(fname) + 2;
	  dprintf(1, PPREFIX"/Func %s\n", fname);
	  for(i= 0; i< nargs; i++) {
	    slen+= strlen(fargs[i]) + 2;
	  }
	  stret= malloc(slen + 512);
	  sprintf(stret, "%s(", fname);
	  for(i= 0; i< nargs; i++) {
	    if (i < nargs - 1) sprintf(buffer, "%s, ", fargs[i]);
	    else sprintf(buffer, "%s", fargs[i]);
	    strcat(stret, buffer);
	  }
	  sprintf(buffer, ")");
	  strcat(stret, buffer);
	  if (!level) printf("%s\n", stret);
	} else {
	  dprintf(1, PPREFIX"FAULTY FUNCTION\n");
	}
	free(buffer);
	for(i= 0; i< nargs; i++) {
	  free(fargs[i]);
	}
	return(stret);
      }
    }    
  }

  free(buffer);
  for(i= 0; i< nargs; i++) {
    free(fargs[i]);
  }
  return(NULL);
}

char *handle_sequence(FILE *fi, int level, int inif, int ineval) 
{
  char *istr, *buffer= malloc(1024);
  char **sarray= (char **) calloc(PARSE_MAX_FUNC_ARGS, sizeof(char*));
  char *stret= NULL;
  int   nargs= 0, i;

  dprintf(1, PPREFIX"handle_sequence(lev%d):\n", level);

  while (istr= fgets(buffer, 1024, fi)) {
    dprintf(2, "::%s", istr);
    if (istr= get1stchar(istr)) {
      if (!strncmp(istr, "<ml:id", 6) && nargs < PARSE_MAX_FUNC_ARGS) {
	dprintf(1, PPREFIX"ml:id: %s\n", istr);
	if (get_id2(istr + 6, &sarray[nargs], NULL) == 0) nargs++;
	else 
	  dprintf(1, PPREFIX"FAULTY boundVar!\n");
      } else if (!strncmp(istr, "<ml:apply", 9)) {
	char *stret2= handle_apply(fi, level + 1, inif, ineval);
	dprintf(1, PPREFIX"ml:apply-f(lev%d): ret %s\n", level, stret2);
	sarray[nargs++]= stret2;
      } else if (!strncmp(istr, "<ml:function", 12)) {
	char *stret2= handle_function(fi, level + 1);
	dprintf(1, PPREFIX"ml:function-f(lev%d): ret %s\n", level, stret2);
	sarray[nargs++]= stret2;
      } else if (!strncmp(istr, "</ml:sequence>", 14)) {
	int slen= 2;
	for(i= 0; i< nargs; i++) {
	  slen+= strlen(sarray[i]) + 2;
	}
	stret= malloc(slen + 512);
	sprintf(stret, "(");
	for(i= 0; i< nargs; i++) {
	  if (i < nargs - 1) sprintf(buffer, "%s, ", sarray[i]);
	  else sprintf(buffer, "%s", sarray[i]);
	  strcat(stret, buffer);
	}
	sprintf(buffer, ")");
	strcat(stret, buffer);
	if (!level) printf("%s\n", stret);

	free(buffer);
	for(i= 0; i< nargs; i++) {
	  free(sarray[i]);
	}
	return(stret);
      }
    }    
  }

  free(buffer);
  for(i= 0; i< nargs; i++) {
    free(sarray[i]);
  }
  return(NULL);
}


char *handle_apply(FILE *fi, int level, int inif, int ineval) 
{
  char *istr, *buffer= malloc(1024);
  char *first= NULL;
  char *second= NULL;
  char *stret= NULL;
  int  action= APPLY_UNKN;
  int  sequence= 0;

  dprintf(1, PPREFIX"ml:apply(lev%d):\n", level);
 
  if (!ineval) {
    while (istr= fgets(buffer, 1024, fi)) {
      dprintf(2, "::%s", istr);
      if (istr= get1stchar(istr)) {
	if (!strncmp(istr, "<ml:indexer", 11)) {
	  dprintf(1, PPREFIX"ml:indexer: %s\n", istr);
	  action= APPLY_INDEXER;
	  break;
	} else if (!strncmp(istr, "<ml:greaterOrEqual", 19)) {
	  dprintf(1, PPREFIX"ml:greaterOrequal: %s\n", istr);
	  action= APPLY_GEQ;
	  break;
	} else if (!strncmp(istr, "<ml:lessThan", 12)) {
	  dprintf(1, PPREFIX"ml:lessThan: %s\n", istr);
	  action= APPLY_LT;
	  break;
	} else if (!strncmp(istr, "<ml:greaterThan", 15)) {
	  dprintf(1, PPREFIX"ml:greaterThan: %s\n", istr);
	  action= APPLY_GT;
	  break;
	} else if (!strncmp(istr, "<ml:lessOrEqual", 15)) {
	  dprintf(1, PPREFIX"ml:lessOrequal: %s\n", istr);
	  action= APPLY_LEQ;
	  break;
	} else if (!strncmp(istr, "<ml:greaterOrEqual", 15)) {
	  dprintf(1, PPREFIX"ml:greaterOrequal: %s\n", istr);
	  action= APPLY_GEQ;
	  break;
	} else if (!strncmp(istr, "<ml:equal", 9)) {
	  dprintf(1, PPREFIX"ml:equal: %s\n", istr);
	  action= APPLY_EQUAL;
	  break;
	} else if (!strncmp(istr, "<ml:plus", 8)) {
	  dprintf(1, PPREFIX"ml:plus: %s\n", istr);
	  action= APPLY_PLUS;
	  break;
	} else if (!strncmp(istr, "<ml:minus", 9)) {
	  dprintf(1, PPREFIX"ml:MINUS: %s\n", istr);
	  action= APPLY_MINUS;
	  break;
	} else if (!strncmp(istr, "<ml:div", 7)) {
	  dprintf(1, PPREFIX"ml:div: %s\n", istr);
	  action= APPLY_DIV;
	  break;
	} else if (!strncmp(istr, "<ml:mult", 8)) {
	  dprintf(1, PPREFIX"ml:mult: %s\n", istr);
	  action= APPLY_MULT;
	  break;
	} else if (!strncmp(istr, "<ml:and", 7)) {
	  dprintf(1, PPREFIX"ml:and: %s\n", istr);
	  action= APPLY_AND;
	  break;
	} else if (!strncmp(istr, "<ml:neg", 7)) {
	  dprintf(1, PPREFIX"ml:neg:\n");
	  action= APPLY_NEG;
	  break;
	} else if (!strncmp(istr, "<ml:pow", 7)) {
	  dprintf(1, PPREFIX"ml:pow: %s\n", istr);
	  action= APPLY_POW;
	  break;
	} else if (!strncmp(istr, "<ml:tanh", 8)) {
	  dprintf(1, PPREFIX"ml:tanh: %s\n", istr);
	  action= APPLY_TANH;
	  break;
	} else if (!strncmp(istr, "<ml:id", 6)) {
	  dprintf(1, PPREFIX"ml:id: %s\n", istr);
	  get_id2(istr + 6, &first, NULL);
	  action= APPLY_UNKN;
	  break;
	}
      }
    }

    if (!istr) {
      free(buffer);
      return(NULL);
    }
  } else { //ineval
    action= APPLY_EVAL;
  }

  while (istr= fgets(buffer, 1024, fi)) {
    dprintf(2, "::%s", istr);
    if (istr= get1stchar(istr)) {
      if (!strncmp(istr, "<ml:id", 6)) {
	dprintf(1, PPREFIX"ml:id: %s\n", istr);
	get_id2(istr + 6, &first, &second);
      } else if (!strncmp(istr, "<ml:real", 8)) {
	dprintf(1, PPREFIX"ml:real: %s\n", istr);
	get_id2(istr + 8, &first, &second);
      } else if (!strncmp(istr, "<ml:parens", 10)) {
	char *stret4;
	dprintf(1, PPREFIX"ml:parens open\n");
	stret4= handle_parens(fi, level + 1);
	if (!first) first= stret4;
	else if (!second) second= stret4;
	else printf("PARENS ERROR\n");
      } else if (!strncmp(istr, "</ml:pow", 7)) {
	dprintf(1, PPREFIX"ml:pow close\n");
      } else if (!strncmp(istr, "<ml:apply", 9)) {
	char *stret2= handle_apply(fi, level + 1, 0, ineval);
	dprintf(1, PPREFIX"ml:apply2(lev%d): ret %s\n", level, stret2);
	if (!first) first= stret2;
	else if (!second) second= stret2;
	else printf("NESTED APPLY ERROR\n");
      } else if (!strncmp(istr, "<ml:sequence", 12)) {
	char *stret2= handle_sequence(fi, level + 1, 0, ineval);
	dprintf(1, PPREFIX"ml:sequence(lev%d): ret %s\n", level, stret2);
	if (!first) {
	  printf("SEQUENCE WITH NO ID\n");
	  first= strdup("UNKNOWN_ID");
	}
	if (!second) second= stret2;
	else printf("NESTED APPLY ERROR\n");
	sequence= 1;
      } else if (!strncmp(istr, "<ml:function", 12)) {
	char *stret2= handle_function(fi, level + 1);
	dprintf(1, PPREFIX"ml:function2(lev%d): ret %s\n", level, stret2);
	if (!first) first= stret2;
	else if (!second) second= stret2;
	else printf("NESTED FUNCTION ERROR\n");
      } else if (!strncmp(istr, "</ml:apply>", 11)) {
	if (first && (second || action == APPLY_NEG)) {
	  if (second) {
	    dprintf(1, PPREFIX"/apply 1=%s 2=%s\n", first, second);
	    stret= malloc(strlen(first) + strlen(second) + 512);
	  } else {
	    dprintf(1, PPREFIX"/apply 1=%s\n", first);
	    stret= malloc(strlen(first) + 512);
	  }
	  switch (action) {
	  case APPLY_EQUAL: {
	    if (!inif) {
	      if (!sequence)
		sprintf(stret, "%s= %s", first, second);
	      else
		sprintf(stret, "%s(%s)", first, second);
	    } else
	      sprintf(stret, "%s == %s", first, second);
	  } break;
	  case APPLY_GEQ: {
	    sprintf(stret, "%s >= %s", first, second);
	  } break;
	  case APPLY_LEQ: {
	    sprintf(stret, "%s <= %s", first, second);
	  } break;
	  case APPLY_LT: {
	    sprintf(stret, "%s < %s", first, second);
	  } break;
	  case APPLY_GT: {
	    sprintf(stret, "%s > %s", first, second);
	  } break;
	  case APPLY_MINUS: {
	    sprintf(stret, "%s - %s", first, second);
	  } break;
	  case APPLY_PLUS: {
	    sprintf(stret, "%s + %s", first, second);
	  } break;
	  case APPLY_MULT: {
	    sprintf(stret, "%s*%s", first, second);
	  } break;
	  case APPLY_AND: {
	    sprintf(stret, "%s&&%s", first, second);
	  } break;
	  case APPLY_DIV: {
	    sprintf(stret, "%s/%s", first, second);
	  } break;
	  case APPLY_POW: {
	    sprintf(stret, "pow(%s, %s)", first, second);
	  } break;
	  case APPLY_TANH: {
	    sprintf(stret, "tanh(%s, %s)", first, second);
	  } break;
	  case APPLY_INDEXER: {
	    sprintf(stret, "%s\[%s]", first, second);
	  } break;
	  case APPLY_NEG: {
	    sprintf(stret, "-(%s)", first);
	  } break;
	  case APPLY_EVAL: {
	    if (!sequence)
	      sprintf(stret, "%s=%s", first, second);
	    else
	      sprintf(stret, "%s(%s)", first, second);
	  } break;
	  default:
	    sprintf(stret, "%s(?%d)%s", first, action, second);	    
	  }
	  if (!level) printf("%s\n", stret);
	} else {
	  dprintf(1, PPREFIX"FAULTY APPLY lev%d act %d\n", level, action);
	}
	free(buffer);
	free(first);
	free(second);
	return(stret);
      }
    }    
  }

  free(buffer);
  free(first);
  free(second);
  return(NULL);
}

#define FUNC_NONE   0
#define FUNC_FUNC   1
#define FUNC_PROG   2

char *handle_define_eval(FILE *fi, int level, int eval) 
{
  char *istr, *buffer= malloc(1024);
  char *first= NULL;
  char *second= NULL;
  char *stret= NULL;
  int   inif= 0;
  int   ifthen= 0;
  int   napply= 0;
  int   isafunc= FUNC_NONE;
  char *accum= NULL;

  dprintf(1, PPREFIX"ml:define(lev%d):\n", level);

  while (istr= fgets(buffer, 1024, fi)) {
    dprintf(2, "::%s", istr);
    if (istr= get1stchar(istr)) {
      if (!strncmp(istr, "<ml:id", 6)) {
	dprintf(1, PPREFIX"ml:id: %s\n", istr);
	if (ifthen) {
	  if (first && accum) {
	    second= NULL;
	    get_id2(istr + 6, &first, &second);
	    sprintf(accum + strlen(accum), "%s= %s;\n", first, second);
	  } else {
	    dprintf(1, PPREFIX"DEFINE FLAW1: accum %x first %s\n", accum, (first) ? first : "NULL");
	  }
	} else {
	  get_id2(istr + 6, &first, &second);
	}
      } else if (!strncmp(istr, "<ml:real", 8)) {
	dprintf(1, PPREFIX"ml:real: %s\n", istr);
	if (ifthen) {
	  if (first && accum) {
	    second= NULL;
	    get_id2(istr + 8, &first, &second);
	    sprintf(accum + strlen(accum), "%s= %s;\n", first, second);
	  } else {
	    dprintf(1, PPREFIX"DEFINE FLAW2: accum %x first %s\n", accum, (first) ? first : "NULL");
	  }
	} else {
	  get_id2(istr + 8, &first, &second);
	}
      } else if (!strncmp(istr, "<ml:result", 10)) {
	get_id2(istr + 10, &first, &second);
      } else if (!strncmp(istr, "<ml:apply", 9)) {
	char *stret3;
	dprintf(1, PPREFIX"ml:apply3(lev%d): ret %s\n", level, stret3);

	if (!napply)
	  stret3= handle_apply(fi, level + 1, inif, eval);
	else
	  stret3= handle_apply(fi, level + 1, 0, eval);

	if (inif) {
	  if (!napply) {
	    if (!accum) {
	      accum= malloc(2048);
	      *accum= 0;
	    }
	    strcat(accum, "if (");
	    strcat(accum, stret3);
	    strcpy(accum + strlen(accum), ") {\n");
	    ifthen= 1;
	  } else {
	    if (accum) {
	      second= stret3;
	      sprintf(accum + strlen(accum), "%s= %s;\n", first, second);
	    } else {
	      dprintf(1, PPREFIX"DEFINE FLAW4: accum %x first %s\n", accum, (first) ? first : "NULL");
	    }
	  }
	} else {
	  if (!first) first= stret3;
	  else if (!second) second= stret3;
	}
	napply++;
      } else if (!strncmp(istr, "<ml:function", 12)) {
	char *stret3= handle_function(fi, 1);
	dprintf(1, PPREFIX"ml:function3(lev%d): ret %s\n", level, stret3);
	if (!first) first= stret3;
	else if (!second) second= stret3;
	isafunc= FUNC_FUNC;
      } else if (!strncmp(istr, "<ml:program", 11)) {
	dprintf(1, PPREFIX"ml:program(lev%d):\n");
	isafunc= FUNC_PROG;
      } else if (!strncmp(istr, "<ml:ifThen", 10)) {
	inif= 1;
	dprintf(1, PPREFIX"ml:if:\n");
      } else if (!strncmp(istr, "</ml:ifThen", 11)) {
	dprintf(1, PPREFIX"ml:if/:\n");
	if (!accum) {
	  accum= malloc(2048);
	  *accum= 0;
	}
	strcat(accum, "}\n");
	inif= 0;
      } else if (!strncmp(istr, "<ml:return", 9)) {
	char *stret5= handle_return(fi, level + 1);
	dprintf(1, PPREFIX"ml:return:\n");
	if (ifthen) {
	  if (first && accum) {
	    second= stret5;
	    sprintf(accum + strlen(accum), "%s= %s;\n", first, second);
	  } else {
	    dprintf(1, PPREFIX"DEFINE FLAW1: accum %x first %s\n", accum, (first) ? first : "NULL");
	  }
	} else {
	  if (!first) first= stret5;
	  else if (!second) second= stret5;
	  else printf("RETURN ERROR\n");
	}
      } else if (!strncmp(istr, "<ml:localDefine", 15)) {
	char *stret3= handle_define_eval(fi, level + 1, 0);
	dprintf(1, PPREFIX"ml:localDefine(lev%d): ret %s\n", level, stret3);
	if (ifthen) {
	  if (accum) {
	    sprintf(accum + strlen(accum), "%s;\n", stret3);
	  } else {
	    dprintf(1, PPREFIX"DEFINE FLAW2: accum %x first %s\n", accum, (first) ? first : "NULL");
	  }
	} else {
	  second= stret3;
	}
      } else if (
		 (!eval && (!strncmp(istr, "</ml:define>", 11) || !strncmp(istr, "</ml:localDefine", 16)))
		 ||
		 (eval && !strncmp(istr, "</ml:eval>", 9))
		 ) {
	if (!eval) {
	  int locd= (strncmp(istr, "</ml:localDefine", 16)) ? 0 : 1;
	  if (accum) {
	    stret= accum;
	    if (!isafunc)
	      printf("%s\n", accum);
	    else {
	      if (isafunc == FUNC_FUNC)
		printf("double %s\n\{\nreturn(%s);\n}\n", first, accum);
	      else {
		printf("double %s\n\{\n%s\n}\n", first, accum);
	      }
	    }
	  } else {
	    if (first && second) {
	      dprintf(1, PPREFIX"/define 1=%s 2=%s\n", first, second);
	      stret= malloc(strlen(first) + strlen(second) + 32);
	      if (!isafunc)
		sprintf(stret, "double %s= %s", first, second);
	      else {
		if (isafunc == FUNC_FUNC)
		  sprintf(stret, "double %s\n{\nreturn(%s);\n}\n", first, second);
		else {
		  sprintf(stret, "double %s\n{\n%s\n}\n", first, second);
		}
	      }
	      if (locd || level)
		strcat(stret, "\n");
	      else
		strcat(stret,";\n");
	    } else {
	      dprintf(1, PPREFIX"FAULTY DEFINE\n");
	    }
	  }
	} else {
	  if (first && second) {
	    dprintf(1, PPREFIX"/eval 1=%s 2=%s\n", first, second);
	    stret= malloc(strlen(first) + strlen(second) + 64);
	    sprintf(stret, "/*eval*/ %s= %s", first, second);
	    if (level)
	      strcat(stret, "\n");
	    else
	      strcat(stret,";\n");
	  } else {
	    dprintf(1, PPREFIX"FAULTY EVAL\n");
	  }
	}
	free(buffer);
	free(first);
	free(second);
	return(stret);
      }
    }    
  }

  free(buffer);
  free(first);
  free(second);
  return(NULL);
}

int main(int argc, char **argv)
{
  FILE *fi;
  FILE *fo;
  char *buffer= malloc(1024);
  char *istr;

  if (argc < 3) return;

  fi= fopen(argv[1], "r");
  if (!fi) return(-1);
  fo= fopen(argv[2], "w");
  if (!fo) return(-2);

  if (!buffer) return(-3);

  while (istr= fgets(buffer, 1024, fi)) {
    dprintf(2, "::%s", istr);
    if (istr= get1stchar(istr)) {
      if (!strncmp(istr, "<ml:define", 10)) {
	char *stret0;
	stret0= handle_define_eval(fi, 0, 0);
	if (stret0)
	  printf("%s\n", stret0);
      } else if (!strncmp(istr, "<ml:eval", 8)) {
	char *stret0;
	stret0= handle_define_eval(fi, 0, 1);
	if (stret0)
	  printf("%s;\n", stret0);
      } else if (!strncmp(istr, "<ml:apply", 9)) {
	char *stret0;
	stret0= handle_apply(fi, 0, 0, 0);
	if (stret0)
	  printf("%s;\n", stret0);
      } else if (!strncmp(istr, "<ml:function", 12)) {
	char *stret0;
	dprintf(1, PPREFIX"ml:function\n");
	stret0= handle_function(fi, 0);
	if (stret0)
	  printf("%s;\n", stret0);
      }
    }
  }
}
