var iframes = document.getElementsByTagName("iframe");
for(var i = 0, j = iframes.length; i < j; ++i){
  iframes[i].width = window.innerWidth * 0.8;
  iframes[i].height = window.innerWidth * 0.8;
}