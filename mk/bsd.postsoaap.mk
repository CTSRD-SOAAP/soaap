.SUFFIXES: .dot .gob .graph .json .pdf
.PHONY: gosoaap
.PRECIOUS:

CLEANFILES+= $(GRAPH).dot $(GRAPH).gob $(GRAPH).graph $(GRAPH).pdf

.json.gob:
	soaap-parse -output=$@ $<

.graph.dot:
	soaap-graph -output=$@ $<

.dot.pdf:
	dot -Tpdf -o $@ $<

.gob.graph:
	soaap-graph -binary -output $@ \
		-analyses=vuln,^privaccess -intersection-depth=1 \
		$<
