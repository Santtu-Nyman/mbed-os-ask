var pdfs = document.getElementsByTagName("embed");
for(var i = 0, j = pdfs.length; i < j; ++i){
  pdfs[i].width = window.innerWidth * 0.8;
  pdfs[i].height = window.innerWidth * 0.8;
}