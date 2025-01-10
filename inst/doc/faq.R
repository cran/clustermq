## ----echo=FALSE, results="hide"-----------------------------------------------
knitr::opts_chunk$set(
    cache = FALSE,
    echo = TRUE,
    collapse = TRUE,
    comment = "#>"
)
options(clustermq.scheduler = "local")
suppressPackageStartupMessages(library(clustermq))

## ----eval=FALSE---------------------------------------------------------------
# > clustermq::Q(identity, x=42, n_jobs=1)
# Submitting 1 worker jobs (ID: cmq8480) ...
# Running 1 calculations (5 objs/19.4 Kb common; 1 calls/chunk) ...

## ----eval=FALSE---------------------------------------------------------------
# options(clustermq.scheduler = "ssh",
#         clustermq.ssh.log = "~/ssh_proxy.log")
# Q(identity, x=1, n_jobs=1)

## ----eval=FALSE---------------------------------------------------------------
# options(clustermq.ssh.timeout = 30) # in seconds

