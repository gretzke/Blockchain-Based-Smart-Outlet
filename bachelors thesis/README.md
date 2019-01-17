#EMS Template for Bachelor/Master Thesis
This is the bachelor and master thesis LaTeX template!

## Download:

``` bash
git clone --recursive https://git.rhrk.uni-kl.de/EIT-Wehn/ThesisTemplate.git
```

## Syntax Highliting

For the syntax highliting you need pygments!
(deb package should be available for your linux distribution)

###Linux & Mac
and then you have also to do this as root (su or sudo):

```bash
easy_install Pygments 
```
### Windows
This thread gives good hints: [link](http://tex.stackexchange.com/questions/23458/how-to-install-syntax-highlight-package-minted-on-windows-7)

### No Syntax Highliting:
If you dont want to use syntax highliting in your thesis find the following lines in doc.tex and comment them:

``` latex
\usepackage{minted}
```
and

``` latex
\newminted{perl}{linenos, bgcolor=light-gray, fontsize=\scriptsize}
\newminted{cpp}{bgcolor=light-gray, fontsize=\scriptsize}
\newminted{tcl}{bgcolor=light-gray, fontsize=\scriptsize}
\newminted{sh}{bgcolor=light-gray, fontsize=\scriptsize}
\newminted{basemake}{bgcolor=light-gray, fontsize=\scriptsize}
```


## Building the Document:
To build the code simply type:

```bash
make
```

## TeX and Figure Handling:
- For each chapter you should create a new tex file, which is stored in the _inc_ folder and will be included in the doc.tex.
- Images should be located in the _img_ folder. You can either store a vector graphic pdf or directly store svg that is converted by *inkscape* automatically when you run make.
