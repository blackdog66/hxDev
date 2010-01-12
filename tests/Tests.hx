
import Geany;

class Tests {
	static function main(){
		trace("intialising tests - this");
	}

	public function new() {
		var doc = Geany.create("nice",null);
		var intext = "woohoo\n";
		doc.setText(intext);
		for (i in 0...5) {
			var pos = doc.getPosition();
			doc.insertText(pos,"new text "+i+"\n");
			pos = doc.getLength();
			doc.insertText(pos,"new length is:"+pos+"\n");
		}
	}
}
