if(typeof require != "undefined" && typeof libopus == "undefined"){
    LIBOPUS_WASM_URL = "../libopus.wasm";
    libopus = require("../libopus.js");
}

libopus.onload = function(){
    var start = new Date().getTime();
    var channels = 1;
    var enc = new libopus.Encoder(channels,48000,8000,20,false);
    var dec = new libopus.Decoder(channels,48000);

    var dataSize = 960 * channels; // 1 sample: 48*20ms
    var samples = new Int16Array(dataSize);
    for(var k = 0; k < dataSize; k++) {
        samples[k] = Math.random()*30000;
    }
    enc.input(samples);
    var frame = enc.output();

    var count = 10;
    var debug = (count <= 10);
    enc.setComplexity(5);
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
if (libopus.loaded) {
    libopus.onload();
}
