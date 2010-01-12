

import gtk.Gtk;
import gtk.Window;
import gtk.Button;
import gtk.VBox;
import gtk.Event;


class TestGtk {

	public static function main() {
		trace("inited TestGtk");
		Gtk.init();
		trace("inside TestGtk");

		var win = new Window(TOPLEVEL);

		trace("past first one");
		var btn = new Button();

		btn.label =  "Quit";
		btn.setBorderWidth(10);


		btn.connect(Event.clicked(function(o) { trace("hello world!"); }));
		btn.connect(Event.clicked(function(o) { Gtk.quit(); }));

		win.add(btn);

		btn.show();
		win.show();

Gtk.run();
	}

	public function new() {
	}


}
