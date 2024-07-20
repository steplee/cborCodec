# Cbor Codec

A decoder and an encoder for the [CBOR serialization standard](https://www.rfc-editor.org/rfc/rfc8949.html#name-major-types).

The `TypedArray` part of [RFC8746](https://www.rfc-editor.org/rfc/rfc8746.html#name-typed-arrays) is also supported, but the multidimensional array part is not.

The API for the decoder is based on the visitor pattern. The API is intended to be small and allow efficient generated code. The reason for writing this library was that other decoder libraries I found either relied on virtual functions (jsoncons), or required decoding to a variant type (nlohmann::json). I only benchmarked jsoncons and it was 300x slower than a flatbuffers decode for a 200 byte object with about 10 keys. Hoping to get to within a few times slower of flatbuffers with this code.

jsoncons seemed to use virtual functions and a few layers of abstractions -- which is great for it as it attempts to implement multiple JSON-like formats. However to make decoding and encoding as stream-lined as possible, this library implements code in the header and does so with no virtual functions and really only one intermediate layer of abstraction.

Benchmarks and more tests to follow...
