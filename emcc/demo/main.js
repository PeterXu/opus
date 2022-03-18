if(typeof require != "undefined" && typeof libac == "undefined"){
    LIBAC_WASM_URL = "../libac.wasm";
    libac = require("../libac.js");
}

// 
// input samples parameters
var sampleRate = 48000;
var channels = 1;
var frameSize = 20; // ms
var sampleSizePerChannel = sampleRate/1000*frameSize;

//
// input samples data
var dataSize = sampleSizePerChannel * channels;
var samples = new Int16Array(dataSize);
for(var k = 0; k < dataSize; k++) {
    samples[k] = Math.random()*30000;
}
console.log("samples size: ", samples.length, samples.BYTES_PER_ELEMENT, samples.byteOffset);

function encodeOne(codec) {
    var enc = null;
    if (codec === "opus") {
        enc = libac.CreateOpusEncoder(frameSize,sampleRate,channels,8000,true);
    } else {
        enc = libac.CreatePcmaEncoder(frameSize);
    }
    enc.setInputParameters(sampleRate, channels);
    enc.setBitrate(64000);
    enc.input(samples);
    var packet = enc.output();
    if (!packet) {
        console.log("no encoding output");
        return null;
    }
    console.log("encode packet size: ", packet.length, packet.BYTES_PER_ELEMENT);
    return packet;
}

function ToExit(dec, enc) {
    enc.destroy();
    dec.destroy();
}

function ToEncode(enc, decoded, debug) {
    enc.input(decoded);
    var data = enc.output();
    while(data){
        if (debug)
            console.log("encoded  "+data.length+" bytes");
        data = enc.output();
    }
}

function ToDecode(dec, encoded, enc, debug) {
    dec.input(encoded);
    var data = dec.output();
    while(data){
        if (debug)
            console.log("decoded "+data.length+" samples");
        ToEncode(enc, data, debug);
        data = dec.output();
    }
}

function testCodec1(codec){
    var packet = encodeOne(codec);
    if (!packet) return;

    var dec = null;
    if (codec === "opus") {
        dec = libac.CreateOpusDecoder(sampleRate,channels);
    } else {
        dec = libac.CreatePcmaDecoder();
    }

    //dec.setOutputParameters(sampleRate, channels);
    //dec.setOutputParameters(8000, 1);
    dec.input(packet);
    var decoded = dec.output();
    if (!decoded) {
        console.log(codec, " no decoding output");
        return;
    }
    console.log(codec, " decode samples size: ", decoded.length);
}

function testCodec2(codec1, codec2){
    var start = new Date().getTime();
    var packet = encodeOne(codec1);
    if (!packet) return;

    // check output codec2
    var enc = null;
    if (codec2 === "opus") {
        enc = libac.CreateOpusEncoder(frameSize,sampleRate,channels,8000,true);
        enc.setComplexity(5);
        //enc.setBitrate(8000);
    } else {
        enc = libac.CreatePcmaEncoder(frameSize);
    }

    // check input codec1
    var dec = null;
    if (codec1 === "opus") {
        var sampleRateIn = sampleRate;
        var channelsIn = channels;
        dec = libac.CreateOpusDecoder(sampleRate, channels);
        dec.setOutputParameters(sampleRateIn, channelsIn);
        enc.setInputParameters(sampleRateIn, channelsIn);
    } else {
        var sampleRateOut = 8000;
        var channelsOut = 1;
        dec = libac.CreatePcmaDecoder();
        dec.setOutputParameters(sampleRate, channels);
        enc.setInputParameters(sampleRateOut, channelsOut);
    }

    var count = 10;
    var debug = (count <= 10);
    console.log("test count:", count);

    var intervalId = setInterval(function() {
        ToDecode(dec, packet, enc, debug);
        count -= 1;
        if (count <= 0) {
            clearInterval(intervalId);
            ToExit(dec, enc);
        }
    }, 50);
}

libac.onload = function(){
    //testCodec1("opus");
    //testCodec1("pcma");
    //testCodec2("opus", "opus");
    testCodec2("opus", "pcma");
}

//if using asm.js, already loaded, force onload
if (libac.loaded) {
    libac.onload();
}
