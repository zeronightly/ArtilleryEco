AMQ (approximate membership query) data structures live underneath every database or query engine you've ever used with the possible exception of some older versions of berkley DB. they're used to eliminate empty queries quickly, or in conjunction with a little bit of design work, to guide queries optimally. 

These two presented here have a variety of properties that make them well suited to our case, but they are not bleeding edge in the space. 

AMQs are directly and deeply related to other probabilitistic bitbashing and hashing algorithms, many of which can be found here in locomo in the Sketch library.

SIMD_Block is highly optimized, and suitable for a wide range of sizes and values.
SingleGate should only be used for sets of about 200 elements or less.
TinyGate117 should only be used for 100 or less, with a real sweet spot around 80.