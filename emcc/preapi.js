Module["onRuntimeInitialized"] = function(){
    if(Module.onload) {
        Module.onload();
    }
    Module.loaded = true;
}

Module["locateFile"] = function(url){
    if(url == "libac.wasm" && typeof LIBAC_WASM_URL != "undefined") {
        return LIBAC_WASM_URL;
    } else {
        return url;
    }
}
