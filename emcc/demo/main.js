if(typeof require != "undefined" && typeof libac == "undefined"){
    LIBAC_WASM_URL = "../libac.wasm";
    libac = require("../libac.js");
}

libac.onload = function(){
    var start = new Date().getTime();
    var channels = 1;
    var codec = libac.Consts.OPUS;
    var enc = new libac.Encoder(codec,20,48000,channels,8000,true);
    var dec = new libac.Decoder(codec,48000,channels);

    var dataSize = 960 * channels; // 1 sample: 48*20ms
    var samples = new Int16Array(dataSize);
    for(var k = 0; k < dataSize; k++) {
        samples[k] = Math.random()*30000;
    }
    enc.setBitrate(64000);
    enc.input(samples);
    var frame = enc.output();
    if (!frame) {
        console.log("no encoding output");
        return;
    }
    console.log("encode size: ", frame.length);

    var count = 10;
    var debug = (count <= 10);
    enc.setComplexity(5);
    enc.setBitrate(8000);
    console.log("test count:", count);

    var fnCheckExit = function() {
        count -= 1;
        if (count <= 0) {
            console.log("time "+(new Date().getTime()-start)+" ms");
            clearInterval(intervalId);
            enc.destroy();
            dec.destroy();
        }
    };
    var fnEncode = function(rawData) {
        enc.input(rawData);
        var data = enc.output();
        while(data){
            if (debug) {
                console.log("encoded  "+data.length+" bytes");
            }
            data = enc.output();
        }
        //fnCheckExit();
    };
    var fnDecode = function(encData) {
        dec.input(encData);
        var osamples = dec.output();
        while(osamples){
            if (debug) {
                console.log("decoded "+osamples.length+" samples");
            }
            fnEncode(osamples);
            osamples = dec.output();
        }
        fnCheckExit();
    };

    var intervalId = setInterval(function() {
        fnDecode(frame);
    }, 50);
}

//if using asm.js, already loaded, force onload
if (libac.loaded) {
    libac.onload();
}
