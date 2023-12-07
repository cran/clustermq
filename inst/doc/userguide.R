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
#  # Recommended:
#  #   If your system has `libzmq` installed but you want to enable the worker crash
#  #   monitor, set the following environment variable to enable compilation of the
#  #   bundled `libzmq` library with the required feature (`-DZMQ_BUILD_DRAFT_API=1`):
#  # Sys.setenv(CLUSTERMQ_USE_SYSTEM_LIBZMQ=0)
#  install.packages('clustermq')

## ----eval=FALSE---------------------------------------------------------------
#  # Sys.setenv(CLUSTERMQ_USE_SYSTEM_LIBZMQ=0)
#  # install.packages('remotes')
#  remotes::install_github('mschubert/clustermq')

## ----eval=FALSE---------------------------------------------------------------
#  # Sys.setenv(CLUSTERMQ_USE_SYSTEM_LIBZMQ=0)
#  # install.packages('remotes')
#  remotes::install_github('mschubert/clustermq', ref="develop")

## ----eval=FALSE---------------------------------------------------------------
#  options(clustermq.scheduler = "your scheduler here")
#  # this may require additional setup, for details see below

## ----eval=FALSE---------------------------------------------------------------
#  # If this is set to 'LOCAL' or 'SSH' you will get the following error:
#  #  Expected PROXY_READY, received ‘PROXY_ERROR: Remote SSH QSys is not allowed’
#  options(
#      clustermq.scheduler = "multiprocess" # or multicore, LSF, SGE, Slurm etc.
#  )

## ----eval=FALSE---------------------------------------------------------------
#  options(
#      clustermq.scheduler = "ssh",
#      clustermq.ssh.host = "user@host", # use your user and host, obviously
#      clustermq.ssh.log = "~/cmq_ssh.log" # log for easier debugging
#  )

## -----------------------------------------------------------------------------
fx = function(x) x * 2
Q(fx, x=1:3, n_jobs=1)

## -----------------------------------------------------------------------------
fx = function(x, y) x * 2 + y
Q(fx, x=1:3, const=list(y=10), n_jobs=1)

## -----------------------------------------------------------------------------
fx = function(x) x * 2 + y
Q(fx, x=1:3, export=list(y=10), n_jobs=1)

## -----------------------------------------------------------------------------
fx = function(x) {
    x %>%
        mutate(area = Sepal.Length * Sepal.Width) %>%
        head()
}
Q(fx, x=list(iris), pkgs="dplyr", n_jobs=1)

## -----------------------------------------------------------------------------
library(foreach)
x = foreach(i=1:3) %do% sqrt(i)

## -----------------------------------------------------------------------------
x = foreach(i=1:3) %dopar% sqrt(i)

## -----------------------------------------------------------------------------
# set up the scheduler first, otherwise this will run sequentially
clustermq::register_dopar_cmq(n_jobs=2, memory=1024) # this accepts same arguments as `Q`
x = foreach(i=1:3) %dopar% sqrt(i) # this will be executed as jobs

## ----eval=FALSE---------------------------------------------------------------
#  library(BiocParallel)
#  register(DoparParam()) # after register_dopar_cmq(...)
#  bplapply(1:3, sqrt)

## ----eval=FALSE---------------------------------------------------------------
#  Q(..., log_worker=TRUE)

## ----eval=FALSE---------------------------------------------------------------
#  Q(..., template=list(log_file = <yourlog>))

## ----eval=FALSE---------------------------------------------------------------
#  options(clustermq.scheduler = "ssh",
#          clustermq.ssh.log = "~/ssh_proxy.log")
#  Q(identity, x=1, n_jobs=1)

## ----eval=FALSE---------------------------------------------------------------
#  options(clustermq.ssh.timeout = 30) # in seconds

## ----eval=FALSE---------------------------------------------------------------
#  Q(..., template=list(bashenv="my environment name"))

## ----eval=FALSE---------------------------------------------------------------
#  options(
#      clustermq.defaults = list(bashenv="my default env")
#  )

## ----eval=FALSE---------------------------------------------------------------
#  options(
#      clustermq.scheduler = "lsf",
#      clustermq.template = "/path/to/file/below" # if using your own template
#  )

## ----eval=FALSE---------------------------------------------------------------
#  options(
#      clustermq.scheduler = "sge",
#      clustermq.template = "/path/to/file/below" # if using your own template
#  )

## ----eval=FALSE---------------------------------------------------------------
#  options(
#      clustermq.scheduler = "slurm",
#      clustermq.template = "/path/to/file/below" # if using your own template
#  )

## ----eval=FALSE---------------------------------------------------------------
#  options(
#      clustermq.scheduler = "pbs",
#      clustermq.template = "/path/to/file/below" # if using your own template
#  )

## ----eval=FALSE---------------------------------------------------------------
#  options(clustermq.scheduler = "Torque",
#          clustermq.template = "/path/to/file/below" # if using your own template
#  )

## ----eval=FALSE---------------------------------------------------------------
#  options(clustermq.scheduler = "ssh",
#  		clustermq.ssh.host = "myhost", # set this up in your local ~/.ssh/config
#          clustermq.ssh.log = "~/ssh_proxy.log", # log file on your HPC
#          clustermq.ssh.timeout = 30, # if changing the default connection timeout
#          clustermq.template = "/path/to/file/below" # if using your own template
#  )

