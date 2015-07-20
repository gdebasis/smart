#ifndef BANGLAH
#define BANGLAH

#define ISCII_OFFSET 128

/* Alphabet */
#define N 161
#define NG 162
#define H 163 /* bishorgo */
#define a 164
#define aa 165
#define i 166
#define ee 167
#define u 168
#define oo 169
#define ri 170
#define e 172
#define oi 173
#define o 176
#define ou 177
#define k 179
#define kh 180
#define g 181
#define gh 182
#define ng 183
#define ch 184
#define chh 185
#define j 186
#define jh 187
#define n 188
#define T 189
#define Th 190
#define D 191
#define Dh 192
#define mn 193
#define t 194
#define th 195
#define d 196
#define dh 197
#define dn 198
#define p 200
#define ph 201
#define b 202
#define bh 203
#define m 204
#define y 205
#define J 206
#define r 207
#define l 209
#define v 212
#define sh 213
#define SH 214
#define s 215
#define h 216
/* matra */
#define AA 218
#define I 219
#define EE 220
#define U 221
#define OO 222
#define RI 223
#define E 225
#define OI 226
#define O 229
#define OU 230
/* others */
#define hosh 232
#define R 233
#define DNARI 234
/* numerals */
#define ZERO 241
#define ONE 242
#define TWO 243
#define THREE 244
#define FOUR 245
#define FIVE 246
#define SIX 247
#define SEVEN 248
#define EIGHT 249
#define NINE 250

/* Character Types */
#define CONSONANT 1
#define VOWEL 2
#define MATRA 3
#define AJOG 4
#define YO 5
#define HOSH 6
#define PHUTKI 7
#define OTHER 20

static char *equivalent[] = {
    "?",	/* 128 */
    "?",	/* 129 */
    "?",	/* 130 */
    "?",	/* 131 */
    "?",	/* 132 */
    "?",	/* 133 */
    "?",	/* 134 */
    "?",	/* 135 */
    "?",	/* 136 */
    "?",	/* 137 */
    "?",	/* 138 */
    "?",	/* 139 */
    "?",	/* 140 */
    "?",	/* 141 */
    "?",	/* 142 */
    "?",	/* 143 */
    "?",	/* 144 */
    "?",	/* 145 */
    "?",	/* 146 */
    "?",	/* 147 */
    "?",	/* 148 */
    "?",	/* 149 */
    "?",	/* 150 */
    "?",	/* 151 */
    "?",	/* 152 */
    "?",	/* 153 */
    "?",	/* 154 */
    "?",	/* 155 */
    "?",	/* 156 */
    "?",	/* 157 */
    "?",	/* 158 */
    "?",	/* 159 */
    "?",	/* 160 */
    ".N",	/* 161 chondrobindu */
    ".n",	/* 162 onushwor */
    "H",	/* 163 bishorgo */
    "a",	/* 164 */
    "aa",	/* 165 */
    "i",	/* 166 */
    "ii",	/* 167 */
    "u",	/* 168 */
    "uu",	/* 169 */
    "R^i",	/* 170 */
    "?",	/* 171 */
    "e",	/* 172 */
    "ai",	/* 173 */
    "?",	/* 174 */
    "?",	/* 175 */
    "o",	/* 176 */
    "au",	/* 177 */
    "?",	/* 178 */
    "k",	/* 179 */
    "kh",	/* 180 */
    "g",	/* 181 */
    "gh",	/* 182 */
    "~N",	/* 183 */
    "ch",	/* 184 */
    "Ch",	/* 185 */
    "j",	/* 186 */
    "jh",	/* 187 */
    "~n",	/* 188 */
    "T",	/* 189 */
    "Th",	/* 190 */
    "D",	/* 191 */
    "Dh",	/* 192 */
    "N",	/* 193 */
    "t",	/* 194 */
    "th",	/* 195 */
    "d",	/* 196 */
    "dh",	/* 197 */
    "n",	/* 198 */
    "?",	/* 199 */
    "p",	/* 200 */
    "ph",	/* 201 */
    "b",	/* 202 */
    "bh",	/* 203 */
    "m",	/* 204 */
    "f",	/* 205 */
    "y",	/* 206 */
    "r",	/* 207 */
    "?",	/* 208 */
    "l",	/* 209 */
    "?",	/* 210 */
    "?",	/* 211 */
    "v",	/* 212 */
    "sh",	/* 213 */
    "Sh",	/* 214 */
    "s",	/* 215 */
    "h",	/* 216 */
    "?",	/* 217 inv */
    "aa",	/* 218 */
    "i",	/* 219 */
    "ii",	/* 220 */
    "u",	/* 221 */
    "uu",	/* 222 */
    "R^i",	/* 223 */
    "?",	/* 224 */
    "e",	/* 225 */
    "ai",	/* 226 */
    "?",	/* 227 */
    "?",	/* 228 */
    "o",	/* 229 */
    "au",	/* 230 */
    "?",	/* 231 */
    "",		/* 232 hoshonto */
    "",		/* 233 R-er phutki */
    ".",	/* 234 */
    "?",	/* 235 */
    "?",	/* 236 */
    "?",	/* 237 */
    "?",	/* 238 */
    "?",	/* 239 ATR */
    "?",	/* 240 EXT */
    "0",	/* 241 */
    "1",	/* 242 */
    "2",	/* 243 */
    "3",	/* 244 */
    "4",	/* 245 */
    "5",	/* 246 */
    "6",	/* 247 */
    "7",	/* 248 */
    "8",	/* 249 */
    "9",	/* 250 */
    "",	/* 251 */
    "",	/* 252 */
    "",	/* 253 */
    "",	/* 254 */
    "",	/* 255 */
};

#endif
