#ifndef BANGLAH
#define BANGLAH

#define ISCII_OFFSET 128

/* Alphabet */
#define N 128
#define NG 129
#define H 130 /* bishorgo */
#define a 131
#define aa 132
#define i 133
#define ee 134
#define u 135
#define oo 136
#define ri 137
#define e 139
#define oi 140
#define o 143
#define ou 144
#define k 146
#define kh 147
#define g 148
#define gh 149
#define ng 150
#define ch 151
#define chh 152
#define j 153
#define jh 154
#define n 155
#define T 156
#define Th 157
#define D 158
#define Dh 159
#define mn 160
#define t 161
#define th 162
#define d 163
#define dh 164
#define dn 165
#define p 167
#define ph 168
#define b 169
#define bh 170
#define m 171
#define y 172
#define J 173
#define r 174
#define l 225
#define v 228
#define sh 229
#define SH 230
#define s 231
#define h 232
/* matra */
#define AA 234
#define I 235
#define EE 236
#define U 237
#define OO 238
#define RI 239
#define E 242
#define OI 243
#define O 246
#define OU 247
/* others */
#define hosh 249
#define R 250
#define DNARI 251

/* Character Types */
#define CONSONANT 1
#define VOWEL 2
#define MATRA 3
#define AJOG 4
#define YO 5
#define HOSH 6
#define OTHER 20

static char *equivalent[] = {
        /* 128 chondrobindu */ "N",
        /* 129 onushwor */ "ng",
        /* 130 bishorgo */ ":",
        /* 131 */ "a",
        /* 132 */ "aa", 
        /* 133 */ "i",
        /* 134 */ "ee",
        /* 135 */ "u",
        /* 136 */ "oo", 
        /* 137 */ "ri",
        /* 138 */ "?",
        /* 139 */ "e",
        /* 140 */ "oi",
        /* 141 */ "?",
        /* 142 */ "?",
        /* 143 */ "o",
        /* 144 */ "ou",
        /* 145 */ "?",
        /* 146 */ "k",
        /* 147 */ "kh",
        /* 148 */ "g",
        /* 149 */ "gh",
        /* 150 */ "ng",
        /* 151 */ "ch",
        /* 152 */ "chh",
        /* 153 */ "j",
        /* 154 */ "jh",
        /* 155 */ "n",
        /* 156 */ "T",
        /* 157 */ "Th",
        /* 158 */ "D",
        /* 159 */ "Dh",
        /* 160 */ "N",
        /* 161 */ "t",
        /* 162 */ "th",
        /* 163 */ "d",
        /* 164 */ "dh",
        /* 165 */ "n",
	/* 166 */ "?",
        /* 167 */ "p",
        /* 168 */ "ph",
        /* 169 */ "b",
        /* 170 */ "bh",
        /* 171 */ "m",
        /* 172 */ "y",
        /* 173 */ "j",
        /* 174 */ "r",
        /* 175 */ "?",
        /* 176 begin table markings */ "?",
        /* 177 */ "?",
        /* 178 */ "?",
        /* 179 */ "?",
        /* 180 */ "?",
        /* 181 */ "?",
        /* 182 */ "?",
        /* 183 */ "?",
        /* 184 */ "?",
        /* 185 */ "?",
        /* 186 */ "?",
        /* 187 */ "?",
        /* 188 */ "?",
        /* 189 */ "?",
        /* 190 */ "?",
        /* 191 */ "?",
        /* 192 */ "?",
        /* 193 */ "?",
        /* 194 */ "?",
        /* 195 */ "?",
        /* 196 */ "?",
        /* 197 */ "?",
        /* 198 */ "?",
        /* 199 */ "?",
        /* 200 */ "?",
        /* 201 */ "?",
        /* 202 */ "?",
        /* 203 */ "?",
        /* 204 */ "?",
        /* 205 */ "?",
        /* 206 */ "?",
        /* 207 */ "?",
        /* 208 */ "?",
        /* 209 */ "?",
        /* 210 */ "?",
        /* 211 */ "?",
        /* 212 */ "?",
        /* 213 */ "?",
        /* 214 */ "?",
        /* 215 */ "?",
        /* 216 */ "?",
        /* 217 */ "?",
        /* 218 */ "?",
        /* 219 */ "?",
        /* 220 */ "?",
        /* 221 */ "?",
        /* 222 */ "?",
        /* 223 end table markings */ "?",
        /* 224 atr */ "?",
        /* 225 */ "l",
        /* 226 */ "?",
        /* 227 */ "?",
        /* 228 */ "w",
        /* 229 */ "sh",
        /* 230 */ "SH",
        /* 231 */ "s",
        /* 232 */ "h",
        /* 233 inv */ "?",
        /* 234 */ "aa",
        /* 235 */ "i",
        /* 236 */ "ee",
        /* 237 */ "u",
        /* 238 */ "oo",
        /* 239 */ "ri",
        /* 240 ext */ "?",
        /* 241 */ "?",
        /* 242 */ "e",
        /* 243 */ "oi",
        /* 244 */ "?",
        /* 245 */ "?",
        /* 246 */ "o",
        /* 247 */ "ou",
        /* 248 */ "?",
        /* 249 hoshonto */ "",
        /* 250 R-er phutki */ "R",
        /* 251 */ ".",
        /* 252 */ "",
        /* 253 */ "",
        /* 254 */ "",
        /* 255 */ "",
};

#endif
