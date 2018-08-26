var thumbnails = document.getElementsByClassName("thumbnail");
var imageViewer = document.getElementsByClassName("image-viewer")[0];
var imageIndex = 0;

(function addThumbnailEventHandlers(){
	for(var i = 0; i < thumbnails.length; ++i){
		thumbnails[i].index = i;
		thumbnails[i].addEventListener("click", function(){
			updateImageViewerSource(this.src);
			updateThumbnailSelection(this);
			updateImageIndex(this.index);
		});
	}
}());


(function addKeyupEventHandlers(){
	document.body.addEventListener("keydown", function(e){
		if(e.keyCode === 37){
			if(imageIndex === 0) updateImageIndex(thumbnails.length-1);
			else updateImageIndex(imageIndex-1);
			updateImageViewerSource(thumbnails[imageIndex].src);
			updateThumbnailSelection(thumbnails[imageIndex]);
		}

		else if(e.keyCode === 39){
			if(imageIndex === thumbnails.length-1) updateImageIndex(0);
			else updateImageIndex(imageIndex+1);
			updateImageViewerSource(thumbnails[imageIndex].src);
			updateThumbnailSelection(thumbnails[imageIndex]);
		}
	});
}());

(function addOnClickEventHandlerForViewer(){
	imageViewer.addEventListener("click", function(e){
		if(imageIndex === thumbnails.length-1) updateImageIndex(0);
		else updateImageIndex(imageIndex+1);
		updateImageViewerSource(thumbnails[imageIndex].src);
		updateThumbnailSelection(thumbnails[imageIndex]);
	});
}());

// might as well call click on the first thumbnail
thumbnails[0].click();

function updateThumbnailSelection(thumbnail){
	for(var i = 0; i < thumbnails.length; ++i){
		thumbnails[i].style.border = "1px solid white";
	}
	
	thumbnail.style.border = "1px solid darkblue";
}

function updateImageViewerSource(src){
	imageViewer.setAttribute("src", src);
}

function updateImageIndex(value){
	imageIndex = value;
}