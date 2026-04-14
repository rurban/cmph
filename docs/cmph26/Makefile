# Makefile for cmph26 arXiv submission
# Targets: pdf (typst), tex (pandoc), tex-pdf (latex), arxiv (submission tarball)

PAPER    = cmph26
TYPST    = typst
PANDOC   = pandoc
PDFLATEX = pdflatex
BIBTEX   = bibtex

.PHONY: all pdf tex tex-pdf arxiv clean distclean

all: pdf tex tex-pdf

## Typst → PDF
pdf: $(PAPER).pdf

$(PAPER).pdf: $(PAPER).typ $(PAPER).bib
	$(TYPST) compile $(PAPER).typ

## Typst → LaTeX (via pandoc; bibliography inlined by citeproc — no .bib needed at submission)
tex: $(PAPER).tex

$(PAPER).tex: $(PAPER).typ $(PAPER).bib
	$(PANDOC) -f typst -t latex --standalone \
	          --bibliography $(PAPER).bib --citeproc \
	          -o $@ $<

## LaTeX → PDF (two-pass; bibliography already inlined by citeproc)
tex-pdf: $(PAPER)-latex.pdf

$(PAPER)-latex.pdf: $(PAPER).tex
	$(PDFLATEX) -interaction=nonstopmode $(PAPER).tex
	$(PDFLATEX) -interaction=nonstopmode $(PAPER).tex
	mv $(PAPER).pdf $@

## arXiv submission tarball: self-contained .tex only (bibliography inlined)
arxiv: $(PAPER)-arxiv.tar.gz

$(PAPER)-arxiv.tar.gz: $(PAPER).tex
	tar czf $@ $(PAPER).tex

clean:
	rm -f $(PAPER).aux $(PAPER).bbl $(PAPER).blg $(PAPER).log \
	       $(PAPER).out $(PAPER).toc $(PAPER).fls $(PAPER).fdb_latexmk

distclean: clean
	rm -f $(PAPER).pdf $(PAPER)-latex.pdf $(PAPER)-arxiv.tar.gz $(PAPER).tex
