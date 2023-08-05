/***** This file contains automatically generated code ******
**
** The code in this file has been automatically generated by
**
**   mkkeywordhash.c
**
** The code in this file implements a function that determines whether
** or not a given identifier is really an SQL keyword.  The same thing
** might be implemented more directly using a hand-written hash table.
** But by using this automatically generated code, the size of the code
** is substantially reduced.  This is important for embedded applications
** on platforms with limited memory.
*/
/* Hash score: 272 */
#ifndef SQL_LEXER_KEYWORD_H
#define SQL_LEXER_KEYWORD_H
static inline int keywordCode(const char *z, int n) {
	/* zText[] encodes 914 bytes of keywords in 606 bytes */
	/*   DESCAPELSELECTABLEFTHENDATABASEXPLAINTERVALUESUBPARTITIONS         */
	/*   PASSWORDEREGEXPLANALYZEXCEPTIMESERIESWITHOUTERAISEXCLUSIVE         */
	/*   XISTSBEGINDEXTRACTIMESTAMPADDAY_OF_WEEKEYBETWEENATURALLIKEVERY     */
	/*   CASEFALSEGROUPDATEJOINNERELEASEMONTHASHAVINGLOBYPRAGMATCHOUR       */
	/*   EVOKEPREVIOUSEROLEVIRTUALTERANGEWHENANOSECONDAY_OF_YEARENAME       */
	/*   WHEREPLACEWITHINSERTIMESTAMPDIFFILLIMITIMESTAMP_ADDELETEXOR        */
	/*   IGHTIMESTAMP_DIFFORCEABORTIMESTAMP_TRUNCASTRANSACTIONONEAFTER      */
	/*   OLLBACKANDISTINCTRUEASCOLLATECOLUMNOTNULLINEAROWCOMMITCONFLICT     */
	/*   CREATECROSSDROPRIMARYFROMICROSECONDFULLGRANTIDENTIFIEDINSTEAD      */
	/*   INTERSECTINTOFFSETISNULLMILLISECONDMINUTEOVERUNIONUSINGVIEW        */
	/*   QUERY                                                              */
	static const char zText[605] = {
		'D','E','S','C','A','P','E','L','S','E','L','E','C','T','A','B','L','E',
		'F','T','H','E','N','D','A','T','A','B','A','S','E','X','P','L','A','I',
		'N','T','E','R','V','A','L','U','E','S','U','B','P','A','R','T','I','T',
		'I','O','N','S','P','A','S','S','W','O','R','D','E','R','E','G','E','X',
		'P','L','A','N','A','L','Y','Z','E','X','C','E','P','T','I','M','E','S',
		'E','R','I','E','S','W','I','T','H','O','U','T','E','R','A','I','S','E',
		'X','C','L','U','S','I','V','E','X','I','S','T','S','B','E','G','I','N',
		'D','E','X','T','R','A','C','T','I','M','E','S','T','A','M','P','A','D',
		'D','A','Y','_','O','F','_','W','E','E','K','E','Y','B','E','T','W','E',
		'E','N','A','T','U','R','A','L','L','I','K','E','V','E','R','Y','C','A',
		'S','E','F','A','L','S','E','G','R','O','U','P','D','A','T','E','J','O',
		'I','N','N','E','R','E','L','E','A','S','E','M','O','N','T','H','A','S',
		'H','A','V','I','N','G','L','O','B','Y','P','R','A','G','M','A','T','C',
		'H','O','U','R','E','V','O','K','E','P','R','E','V','I','O','U','S','E',
		'R','O','L','E','V','I','R','T','U','A','L','T','E','R','A','N','G','E',
		'W','H','E','N','A','N','O','S','E','C','O','N','D','A','Y','_','O','F',
		'_','Y','E','A','R','E','N','A','M','E','W','H','E','R','E','P','L','A',
		'C','E','W','I','T','H','I','N','S','E','R','T','I','M','E','S','T','A',
		'M','P','D','I','F','F','I','L','L','I','M','I','T','I','M','E','S','T',
		'A','M','P','_','A','D','D','E','L','E','T','E','X','O','R','I','G','H',
		'T','I','M','E','S','T','A','M','P','_','D','I','F','F','O','R','C','E',
		'A','B','O','R','T','I','M','E','S','T','A','M','P','_','T','R','U','N',
		'C','A','S','T','R','A','N','S','A','C','T','I','O','N','O','N','E','A',
		'F','T','E','R','O','L','L','B','A','C','K','A','N','D','I','S','T','I',
		'N','C','T','R','U','E','A','S','C','O','L','L','A','T','E','C','O','L',
		'U','M','N','O','T','N','U','L','L','I','N','E','A','R','O','W','C','O',
		'M','M','I','T','C','O','N','F','L','I','C','T','C','R','E','A','T','E',
		'C','R','O','S','S','D','R','O','P','R','I','M','A','R','Y','F','R','O',
		'M','I','C','R','O','S','E','C','O','N','D','F','U','L','L','G','R','A',
		'N','T','I','D','E','N','T','I','F','I','E','D','I','N','S','T','E','A',
		'D','I','N','T','E','R','S','E','C','T','I','N','T','O','F','F','S','E',
		'T','I','S','N','U','L','L','M','I','L','L','I','S','E','C','O','N','D',
		'M','I','N','U','T','E','O','V','E','R','U','N','I','O','N','U','S','I',
		'N','G','V','I','E','W','Q','U','E','R','Y',
	};
	static const unsigned char aHash[127] = {
		  76, 127,  95, 117,  26,   0,   0,  61,  28,   0,  57,  14,   0,
		  70,   0,  23, 118,  75,  92,  66, 126,  86,   0,   4,   0,  13,
		   0,   0, 131,  52, 129,  67, 122,   0,   0,   0,   0, 100, 109,
		   0,  44,   0,   0, 134,  96, 112,   0, 106,   0,   0,   0,  87,
		   0,   0,  83,  71,   9,   0, 133,   0,   8,   0,   2,  54,   3,
		 101,   0,   0,  41, 111, 116,   0,  33,   0,  46,   0,  32,   0,
		  88,  25,  35, 110,   0,   0,   0,  91,   0, 107,  94, 115,  10,
		  37, 113,   0,  89,   0,  19, 105,  72,  65,  62, 132,  56, 128,
		 103,  21,  79,   0,   0, 120, 121, 130,  77,  36, 119,  58,   0,
		  17,   0,  90,  49, 104,   0,  43, 102,   0,  47,
  };
	static const unsigned char aNext[134] = {
		   0,   1,   0,   0,   0,   0,   0,   0,   0,   0,   6,   0,   0,
		   0,   0,   0,   0,   0,   0,   0,  11,   7,   0,   0,   0,  16,
		   0,  12,  20,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
		   0,   0,   0,   5,   0,  31,  34,   0,   0,   0,  27,   0,   0,
		  15,   0,   0,   0,  51,   0,  55,   0,   0,   0,  42,   0,  30,
		   0,  22,   0,   0,   0,   0,  59,   0,  50,   0,   0,  29,   0,
		   0,   0,   0,   0,   0,  60,   0,   0,  45,  68,   0,  73,   0,
		  84,  53,  64,   0,  81,  38,  40,   0,   0,   0,  98,   0,  69,
		  74,  99,   0,  78,   0,   0,  97,  24,   0,  93, 108,  39,   0,
		   0,  80,  85,   0,   0,  48, 114, 123, 124,   0, 125,   0,   0,
		  63,  18,  82,   0,
  };
	static const unsigned char aLen[134] = {
		   4,   6,   4,   6,   5,   4,   4,   3,   8,   7,   8,   6,  13,
		  12,  10,   9,   2,   8,   2,   5,   2,   6,   4,   7,   6,  10,
		   4,   7,   5,   5,   9,   6,   5,   5,   7,  12,  11,   3,   7,
		   7,   3,   4,   5,   4,   5,   5,   6,   4,   5,   7,   5,   4,
		   6,   4,   2,   6,   5,   4,   6,   8,   4,   4,   7,   5,   5,
		   4,  10,  11,   3,   2,   4,   6,   5,   7,   6,   6,  13,   4,
		   5,  13,   3,   6,   3,   5,  14,   2,   5,   3,   5,  15,   4,
		  11,   4,   5,   8,   3,   8,   4,   3,   7,   6,   7,   3,   4,
		   6,   3,   6,   8,   6,   5,   4,   7,   4,  11,   4,   5,  10,
		   7,   9,   2,   4,   2,   6,   3,   6,  11,   2,   6,   6,   4,
		   5,   5,   4,   5,
  };
	static const unsigned short int aOffset[134] = {
		   0,   1,   6,   8,  13,  16,  19,  21,  23,  30,  35,  40,  45,
		  45,  48,  48,  55,  58,  59,  63,  63,  67,  72,  74,  80,  85,
		  95,  95,  99, 103, 107, 115, 121, 124, 127, 133, 144, 154, 157,
		 163, 168, 170, 173, 178, 182, 187, 190, 196, 198, 202, 209, 213,
		 216, 221, 224, 226, 230, 234, 237, 243, 249, 252, 256, 261, 265,
		 270, 273, 282, 282, 286, 289, 292, 298, 301, 308, 312, 317, 329,
		 332, 336, 346, 348, 354, 356, 360, 371, 373, 373, 378, 382, 396,
		 399, 409, 413, 417, 425, 427, 434, 438, 440, 447, 452, 452, 455,
		 458, 463, 466, 472, 480, 486, 491, 494, 501, 504, 515, 519, 524,
		 534, 541, 541, 550, 552, 553, 556, 559, 565, 569, 570, 576, 582,
		 586, 591, 596, 600,
  };
	static const unsigned char aCode[134] = {
		TK_DESC,       TK_ESCAPE,     TK_ELSE,       TK_SELECT,     TK_TABLE,      
		TK_LEFT,       TK_THEN,       TK_END,        TK_DATABASE,   TK_EXPLAIN,    
		TK_INTERVAL,   TK_VALUES,     TK_SUBPARTITIONS,  TK_SUBPARTITION, TK_PARTITIONS, 
		TK_PARTITION,  TK_ON,         TK_PASSWORD,   TK_AS,         TK_ORDER,      
		TK_OR,         TK_LIKE_KW,    TK_PLAN,       TK_ANALYZE,    TK_EXCEPT,     
		TK_TIMESERIES, TK_WITH,       TK_WITHOUT,    TK_OUTER,      TK_RAISE,      
		TK_EXCLUSIVE,  TK_EXISTS,     TK_BEGIN,      TK_INDEX,      TK_EXTRACT,    
		TK_TIMESTAMPADD, TK_DAY_OF_WEEK,TK_KEY,        TK_BETWEEN,    TK_NATURAL,    
		TK_ALL,        TK_LIKE_KW,    TK_EVERY,      TK_CASE,       TK_FALSE,      
		TK_GROUP,      TK_UPDATE,     TK_JOIN,       TK_INNER,      TK_RELEASE,    
		TK_MONTH,      TK_HASH,       TK_HAVING,     TK_LIKE_KW,    TK_BY,         
		TK_PRAGMA,     TK_MATCH,      TK_HOUR,       TK_REVOKE,     TK_PREVIOUS,   
		TK_USER,       TK_ROLE,       TK_VIRTUAL,    TK_ALTER,      TK_RANGE,      
		TK_WHEN,       TK_NANOSECOND, TK_DAY_OF_YEAR,TK_DAY,        TK_OF,         
		TK_YEAR,       TK_RENAME,     TK_WHERE,      TK_REPLACE,    TK_WITHIN,     
		TK_INSERT,     TK_TIMESTAMPDIFF,  TK_FILL,       TK_LIMIT,      TK_TIMESTAMPADD, 
		TK_ADD,        TK_DELETE,     TK_XOR,        TK_RIGHT,      TK_TIMESTAMPDIFF,  
		TK_IF,         TK_FORCE,      TK_FOR,        TK_ABORT,      TK_TIMESTAMP_TRUNC,    
		TK_CAST,       TK_TRANSACTION,TK_NONE,       TK_AFTER,      TK_ROLLBACK,   
		TK_AND,        TK_DISTINCT,   TK_TRUE,       TK_ASC,        TK_COLLATE,    
		TK_COLUMNKW,   TK_NOTNULL,    TK_NOT,        TK_NULL,       TK_LINEAR,     
		TK_ROW,        TK_COMMIT,     TK_CONFLICT,   TK_CREATE,     TK_CROSS,      
		TK_DROP,       TK_PRIMARY,    TK_FROM,       TK_MICROSECOND,TK_FULL,       
		TK_GRANT,      TK_IDENTIFIED, TK_INSTEAD,    TK_INTERSECT,  TK_IN,         
		TK_INTO,       TK_TO,         TK_OFFSET,     TK_SET,        TK_ISNULL,     
		TK_MILLISECOND,TK_IS,         TK_SECOND,     TK_MINUTE,     TK_OVER,       
		TK_UNION,      TK_USING,      TK_VIEW,       TK_QUERY,      
  };
	int h, i;
	if (n < 2) return TK_ID;
	h = ((SQLLexer::charMap(z[0]) * 4) ^
			(SQLLexer::charMap(z[n-1]) * 3) ^
			n) % 127;
	for (i = ((int)aHash[h]) - 1; i >= 0; i = ((int)aNext[i]) - 1) {
		if (aLen[i] == n && gsStrNICmp(&zText[aOffset[i]], z, n)==0) {
			return aCode[i];
		}
	}
	return TK_ID;
}
#define GS_N_KEYWORD 134
#endif