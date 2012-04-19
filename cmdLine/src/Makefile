GUI_FILES= \
		ui/wizard.ui \
		ui/wizard_generic.ui \
		ui/rep_output.ui \
		ui/file_display.ui

GUI_PYTHON_FILES := $(patsubst %.ui,%.py,$(GUI_FILES))

ui/%.py : ui/%.ui
	pyuic4 $< -o $@

all: $(GUI_PYTHON_FILES)
	@echo Building Repertoire UI
	@touch ui/__init__.py

clean:
	rm -f ui/*.py
	rm -f ui/*.pyc
	rm -f *.pyc
	rm -rf repertoire_tmp_*
