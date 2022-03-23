/// codec list
var Consts = {
    OPUS: 1,
    PCMA: 2,
    PCMU: 3,
};
Module.Consts = Consts;

/// create encoders
Module.CreateOpusEncoder = function(frame_size, samplerate, channels, bitrate, is_voip) {
    return new Encoder(Consts.OPUS, frame_size, samplerate, channels, bitrate, is_voip);
}
Module.CreatePcmaEncoder = function(frame_size) {
    return new Encoder(Consts.PCMA, frame_size);
}
Module.CreatePcmuEncoder = function(frame_size) {
    return new Encoder(Consts.PCMU, frame_size);
}

// create decoders
Module.CreateOpusDecoder = function(samplerate, channels) {
    return new Decoder(Consts.OPUS, samplerate, channels);
}
Module.CreatePcmaDecoder = function() {
    return new Decoder(Consts.PCMA);
}
Module.CreatePcmuDecoder = function() {
    return new Decoder(Consts.PCMU);
}


/// Encoder

// create encoder
// @param codec: opus/pcma/pcmu
// @param frame_size: frame size in milliseconds (2.5,5,10,20,40,60), 20 is recommended
// @param samplerate: 8000,12000,16000,24000,48000 [g711 unsupport]
// @param channels: 1-2, [g711 unsupport]
// @param bitrate: see Opus recommended bitrates (bps), [g711 unsupport]
// @param is_voip: true(voip)/false(audio), [g711 unsupport]
function Encoder(codec, frame_size, samplerate, channels, bitrate, is_voip)
{
    this.enc = Module._Encoder_new.apply(null, arguments);
    this.out = Module._String_new();
}

// free encoder
Encoder.prototype.destroy = function()
{ 
    Module._Encoder_delete(this.enc); 
    Module._String_delete(this.out);
}

// set complexity: 0~10, 10 is the most complexity.
Encoder.prototype.setComplexity = function(complexity)
{
    Module._Encoder_setComplexity(this.enc, complexity);
}

// set bitrate: 0 is auto-detect.
Encoder.prototype.setBitrate = function(bitrate)
{
    Module._Encoder_setBitrate(this.enc, bitrate);
}

// add samples to the encoder buffer. (samples.BYTES_PER_ELEMENT is 2)
// @param samples: Int16Array of interleaved (if multiple channels) samples
Encoder.prototype.input = function(samples, samplerate, channels)
{
    var nbytes = samples.length*samples.BYTES_PER_ELEMENT;
    var ptr = Module._malloc(nbytes);
    var pdata = new Uint8Array(Module.HEAPU8.buffer, ptr, nbytes);
    pdata.set(new Uint8Array(samples.buffer, samples.byteOffset, nbytes));

    var iret = Module._Encoder_input(this.enc, ptr, samples.length, samplerate, channels);
    Module._free(ptr);
    return iret;
}

// output the next encoded packet
// return Uint8Array (valid until the next output call) or null if there is no packet to output
Encoder.prototype.output = function()
{
    var ok = Module._Encoder_output(this.enc, this.out);
    if (ok) {
        return new Uint8Array(Module.HEAPU8.buffer, Module._String_data(this.out), Module._String_size(this.out));
    } else {
        return null;
    }
}


/// Decoder

// create decoder
// @param codec: opus/pcma/pcmu
// @param samplerate: input packet's sample rate, [g711 unsupport]
// @param channels: input packet's channels, [g711 unsupport]
function Decoder(codec, samplerate, channels)
{
    this.dec = Module._Decoder_new.apply(null, arguments);
    this.out = Module._Int16Array_new();
}

// free decoder
Decoder.prototype.destroy = function()
{ 
    Module._Decoder_delete(this.dec); 
    Module._Int16Array_delete(this.out);
}

// add packet to the decoder buffer. (packet.BYTES_PER_ELEMENT is 1)
// @param packet: Uint8Array
Decoder.prototype.input = function(packet)
{
    var nbytes = packet.length*packet.BYTES_PER_ELEMENT;
    var ptr = Module._malloc(nbytes);
    var pdata = new Uint8Array(Module.HEAPU8.buffer, ptr, nbytes);
    pdata.set(new Uint8Array(packet.buffer, packet.byteOffset, nbytes));

    var iret = Module._Decoder_input(this.dec, ptr, packet.length);
    Module._free(ptr);
    return iret;
}

// output the next decoded samples
// return samples (interleaved if multiple channels) as Int16Array (valid until next output call) or null if no output
Decoder.prototype.output = function(samplerate, channels)
{
    var ok = Module._Decoder_output(this.dec, this.out, samplerate, channels);
    if (ok) {
        return new Int16Array(Module.HEAPU8.buffer, Module._Int16Array_data(this.out), Module._Int16Array_size(this.out));
    } else {
        return null;
    }
}


//export objects
Module.Encoder = Encoder;
Module.Decoder = Decoder;



/// LocalStream

// create local-stream
function LocalStream()
{
    this.stream = Module._LocalStream_new.apply(null, arguments);
    this.out = Module._String_new();
}

// free local-stream
LocalStream.prototype.destroy = function()
{ 
    Module._LocalStream_delete(this.stream); 
    Module._String_delete(this.out);
}

// set local-stream codec parameters
// @param codec: opus/pcma/pcmu
// @param frame_size: frame size in milliseconds (2.5,5,10,20,40,60), 20 is recommended
// @param samplerate: 8000,12000,16000,24000,48000 [g711 unsupport]
// @param channels: 1-2, [g711 unsupport]
// @param bitrate: see Opus recommended bitrates (bps), [g711 unsupport]
// @param is_voip: true(voip)/false(audio), [g711 unsupport]
LocalStream.prototype.setCodecParameters = function(codec, frame_size, samplerate, channels, bitrate, is_voip)
{
    Module._LocalStream_setCodecParameters(this.stream, codec, frame_size, samplerate, channels, bitrate, is_voip);
}

// set local-stream codec bitrate: 0 is auto-detect.
LocalStream.prototype.setCodecBitrate = function(bitrate)
{
    Module._LocalStream_setCodecBitrate(this.stream, bitrate);
}

// set local-stream rtp parameters
LocalStream.prototype.setRtpParameters = function(ssrc, payloadtype)
{
    Module._LocalStream_setRtpParameters(this.stream, ssrc, payloadtype);
}

// add samples to the local-stream encoder buffer. (samples.BYTES_PER_ELEMENT is 2)
// @param samples: Int16Array of interleaved (if multiple channels) samples
LocalStream.prototype.input = function(samples, samplerate, channels)
{
    var nbytes = samples.length*samples.BYTES_PER_ELEMENT;
    var ptr = Module._malloc(nbytes);
    var pdata = new Uint8Array(Module.HEAPU8.buffer, ptr, nbytes);
    pdata.set(new Uint8Array(samples.buffer, samples.byteOffset, nbytes));

    var iret = Module._LocalStream_input(this.stream, ptr, samples.length, samplerate, channels);
    Module._free(ptr);
    return iret;
}

// add samples to the local-stream encoder buffer. (samples.BYTES_PER_ELEMENT is 4)
// @param samples: Float32Array of planar (if multiple channels) samples
LocalStream.prototype.input2 = function(samples, sampleRate, channels) {
    var nbytes = samples.length*samples.BYTES_PER_ELEMENT;
    var ptr = Module._malloc(nbytes);
    var pdata = new Uint8Array(Module.HEAPU8.buffer, ptr, nbytes);
    pdata.set(new Uint8Array(samples.buffer, samples.byteOffset, nbytes));

    var iret = Module._LocalStream_input2(this.stream, ptr, samples.length, sampleRate, channels);
    Module._free(ptr);
    return iret;
}

// output the next encoded packet
// return Uint8Array (valid until the next output call) or null if there is no packet to output
LocalStream.prototype.output = function()
{
    var ok = Module._LocalStream_output(this.stream, this.out);
    if (ok) {
        return new Uint8Array(Module.HEAPU8.buffer, Module._String_data(this.out), Module._String_size(this.out));
    } else {
        return null;
    }
}


/// RemoteStream

// create remote-stream
function RemoteStream()
{
    this.stream = Module._RemoteStream_new.apply(null, arguments);
    this.out = Module._Int16Array_new();
}

// free remote-stream
RemoteStream.prototype.destroy = function()
{ 
    Module._RemoteStream_delete(this.stream); 
    Module._Int16Array_delete(this.out);
}

// register remote-stream mapping of payload-type => codec.
// @param payloadtype: 0~127
// @param codec: opus/pcma/pcmu
// @param samplerate: input packet's sample rate, [g711 unsupport]
// @param channels: input packets' channels, [g711 unsupport]
RemoteStream.prototype.registerPayloadType = function(payloadtype, codec, samplerate, channels)
{
    Module._RemoteStream_registerPayloadType(this.stream, payloadtype, codec, samplerate, channels);
}

// add packet to the remote-stream's decoder buffer. (packet.BYTES_PER_ELEMENT is 1)
// @param packet: Uint8Array
RemoteStream.prototype.input = function(packet)
{
    var nbytes = packet.length*packet.BYTES_PER_ELEMENT;
    var ptr = Module._malloc(nbytes);
    var pdata = new Uint8Array(Module.HEAPU8.buffer, ptr, nbytes);
    pdata.set(new Uint8Array(packet.buffer, packet.byteOffset, nbytes));

    var iret = Module._RemoteStream_input(this.stream, ptr, packet.length);
    Module._free(ptr);
    return iret;
}

// output the next decoded samples
// return samples (interleaved if multiple channels) as Int16Array (valid until next output call) or null if no output
RemoteStream.prototype.output = function(samplerate, channels)
{
    var ok = Module._RemoteStream_output(this.stream, this.out, samplerate, channels);
    if (ok) {
        return new Int16Array(Module.HEAPU8.buffer, Module._Int16Array_data(this.out), Module._Int16Array_size(this.out));
    } else {
        return null;
    }
}


//export objects
Module.LocalStream = LocalStream;
Module.RemoteStream = RemoteStream;


var libac = Module;
if (libac.loaded) {
    libac.onload();
}
//export default libac;
