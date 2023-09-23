## ----echo=FALSE, results="hide"-----------------------------------------------
knitr::opts_chunk$set(
    cache = FALSE,
    echo = TRUE,
    collapse = TRUE,
    comment = "#>"
)
options(clustermq.scheduler = "local")
library(clustermq)

## ----eval=FALSE---------------------------------------------------------------
#  # start up a pool of three workers using the default scheduler
#  w = workers(n_jobs=3)
#  
#  # if we make an unclean exit for whatever reason, clean up the jobs
#  on.exit(w$cleanup())

## ----eval=FALSE---------------------------------------------------------------
#  msg = w$recv() # this will block until a worker is ready

## ----eval=FALSE---------------------------------------------------------------
#  w$send(expression, ...)

## ----eval=FALSE---------------------------------------------------------------
#  w$send(clustermq:::work_chunk(chunk, fun, const, rettype, common_seed),
#         chunk = chunk(iter, submit_index))

## ----eval=FALSE---------------------------------------------------------------
#  w$env(object=value, ...)

## ----eval=FALSE---------------------------------------------------------------
#  w = workers(3)
#  on.exit(w$cleanup())
#  w$env(...)
#  
#  while (we have new work to send || jobs pending) {
#      res = w$recv()
#      # handle result
#  
#      if (more work)
#          w$send(expression, ...)
#      else
#          w$send_shutdown()
#  }

## ----eval=FALSE---------------------------------------------------------------
#  R_tryEvalSilent(cmd, env, &err);

