## ----include=FALSE-------------------------------------------------------
options(clustermq.scheduler = 'local')
library(clustermq)

## ----eval=FALSE----------------------------------------------------------
#  # load the library and create a simple function
#  library(clustermq)
#  fx = function(x) x * 2
#  
#  # queue the function call
#  Q(fx, x=1:3, n_jobs=1)
#  
#  # list(2,4,6)
#  
#  # this will submit a cluster job that connects to the master via TCP
#  # the master will then send the function and argument chunks to the worker
#  # and the worker will return the results to the master
#  # until everything is done and you get back your result
#  
#  # we can also use dplyr's mutate to modify data frames
#  library(dplyr)
#  iris %>%
#      mutate(area = Q(`*`, e1=Sepal.Length, e2=Sepal.Width, n_jobs=1))

## ----eval=FALSE----------------------------------------------------------
#  install.packages('clustermq')

## ----eval=FALSE----------------------------------------------------------
#  # install.packages('devtools')
#  devtools::install_github('mschubert/clustermq', ref="develop")

## ------------------------------------------------------------------------
fx = function(x) x * 2
Q(fx, x=1:3, n_jobs=1)

## ------------------------------------------------------------------------
fx = function(x, y) x * 2 + y
Q(fx, x=1:3, const=list(y=10), n_jobs=1)

## ------------------------------------------------------------------------
fx = function(x) x * 2 + y
Q(fx, x=1:3, export=list(y=10), n_jobs=1)

## ------------------------------------------------------------------------
fx = function(x) {
    library(dplyr)
    x %>%
        mutate(area = Sepal.Length *Sepal.Width) %>%
        head()
}
Q(fx, x=list(iris), n_jobs=1)

