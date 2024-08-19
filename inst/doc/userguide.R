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
#  #   If your system has `libzmq` installed but you want to enable the worker
#  #   crash monitor, set the environment variable below to use the bundled
#  #   `libzmq` library with the required feature (`-DZMQ_BUILD_DRAFT_API=1`):
#  
#  # Sys.setenv(CLUSTERMQ_USE_SYSTEM_LIBZMQ=0)
#  install.packages("clustermq")

## ----eval=FALSE---------------------------------------------------------------
#  # Sys.setenv(CLUSTERMQ_USE_SYSTEM_LIBZMQ=0)
#  # install.packages('remotes')
#  remotes::install_github("mschubert/clustermq")
#  # remotes::install_github("mschubert/clustermq@develop") # dev version

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
f1 = function(x) splitIndices(x, 3)
Q(f1, x=3, n_jobs=1, pkgs="parallel")

f2 = function(x) parallel::splitIndices(x, 3)
Q(f2, x=8, n_jobs=1)

# Q(f1, x=5, n_jobs=1)
# (Error #1) could not find function "splitIndices"

## -----------------------------------------------------------------------------
library(foreach)
foreach(i=1:3) %do% sqrt(i)

## -----------------------------------------------------------------------------
foreach(i=1:3) %dopar% sqrt(i)

## -----------------------------------------------------------------------------
# set up the scheduler first, otherwise this will run sequentially

# this accepts same arguments as `Q`
clustermq::register_dopar_cmq(n_jobs=2, memory=1024)

# this will be executed as jobs
foreach(i=1:3) %dopar% sqrt(i)

## ----eval=FALSE---------------------------------------------------------------
#  library(BiocParallel)
#  
#  # the number of jobs is ignored here since we're using the LOCAL scheduler
#  clustermq::register_dopar_cmq(n_jobs=2, memory=1024)
#  register(DoparParam())
#  bplapply(1:3, sqrt)

## ----eval=FALSE---------------------------------------------------------------
#  Q(..., log_worker=TRUE)

## ----eval=FALSE---------------------------------------------------------------
#  Q(..., template=list(log_file = <yourlog>))

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
#  options(
#      clustermq.scheduler = "Torque",
#      clustermq.template = "/path/to/file/below" # if using your own template
#  )

## ----eval=FALSE---------------------------------------------------------------
#  options(
#      clustermq.scheduler = "ssh",
#      clustermq.ssh.host = "myhost", # set this up in your local ~/.ssh/config
#      clustermq.ssh.log = "~/ssh_proxy.log",     # log file on your HPC
#      clustermq.ssh.timeout = 30,    # if changing default connection timeout
#      clustermq.template = "/path/to/file/below" # if using your own template
#  )

