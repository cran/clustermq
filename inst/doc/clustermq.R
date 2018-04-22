## ----include=FALSE-------------------------------------------------------
options(clustermq.scheduler = 'local')
library(clustermq)

## ----eval=FALSE----------------------------------------------------------
#  # load the library and create a simple function
#  library(clustermq)
#  fx = function(x) x * 2
#  
#  # queue the function call on your scheduler
#  Q(fx, x=1:3, n_jobs=1)
#  # list(2,4,6)

## ----eval=FALSE----------------------------------------------------------
#  install.packages('clustermq')

## ----eval=FALSE----------------------------------------------------------
#  # install.packages('devtools')
#  devtools::install_github('mschubert/clustermq')
#  # devtools::install_github('mschubert/clustermq', ref="develop") # dev version

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
        mutate(area = Sepal.Length * Sepal.Width) %>%
        head()
}
Q(fx, x=list(iris), n_jobs=1)

