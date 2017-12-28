/*
Copyright 2016 Google Inc. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

Author: lode.vandevenne@gmail.com (Lode Vandevenne)
Author: jyrki.alakuijala@gmail.com (Jyrki Alakuijala)
*/

/*
Utilities for using the lz77 symbols of the deflate spec.
*/

#ifndef ZOPFLI_SYMBOLS_H_
#define ZOPFLI_SYMBOLS_H_

/* Gets the amount of extra bits for the given dist, cfr. the DEFLATE spec. */
int ZopfliGetDistExtraBits(int dist);

/* Gets value of the extra bits for the given dist, cfr. the DEFLATE spec. */
int ZopfliGetDistExtraBitsValue(int dist);

/* Gets the symbol for the given dist, cfr. the DEFLATE spec. */
int ZopfliGetDistSymbol(int dist);

/* Gets the amount of extra bits for the given length, cfr. the DEFLATE spec. */
int ZopfliGetLengthExtraBits(int l);

/* Gets value of the extra bits for the given length, cfr. the DEFLATE spec. */
int ZopfliGetLengthExtraBitsValue(int l);

/*
Gets the symbol for the given length, cfr. the DEFLATE spec.
Returns the symbol in the range [257-285] (inclusive)
*/
int ZopfliGetLengthSymbol(int l);

/* Gets the amount of extra bits for the given length symbol. */
int ZopfliGetLengthSymbolExtraBits(int s);

/* Gets the amount of extra bits for the given distance symbol. */
int ZopfliGetDistSymbolExtraBits(int s);

#endif  /* ZOPFLI_SYMBOLS_H_ */
