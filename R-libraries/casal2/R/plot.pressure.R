#' @title plot.pressure plot fishing pressure if there has been an exploitation process reported.
#'
#' @description
#' A plotting function to plot fishing presuure (U's )for the 'casal2TAB' and 'casal2MPD' objects.
#'
#' @author Craig Marsh
#' @param model <casal2MPD, casal2TAB> object that are generated from one of the extract.mpd() and extract.tabular() functions.
#' @param report_label <string>
#' @param plot.it Whether to generate a default plot or return the values as a matrix.
#' @param ... remaining plotting options
#' @return generate a plot over time if plot.it = T, if plot.it = F it will return a matrix of values.
#' @rdname plot.pressure
#' @export plot.pressure
#'

"plot.pressure" <-
function(model, report_label="", xlim, ylim, xlab, ylab, main, col, plot.it = T, ...) {
  UseMethod("plot.pressure",model)
}

#' @return \code{NULL}
#'
#' @rdname plot.pressure
#' @method plot.pressure casal2MPD
#' @export
"plot.pressure.casal2MPD" = function(model, report_label="", xlim = NULL, ylim = NULL, xlab = NULL, ylab = NULL, main = NULL, col = NULL ,plot.it = T, ...) {
  muliple_iterations_in_a_report = FALSE
  N_runs = 1
  temp_DF = NULL

  ## check report label exists
  if (!report_label %in% names(model))
    stop(Paste("The report label '", report_label, "' was not found. The report labels available are: ", paste(names(model),collapse = ", ")))

  ## get the report out
  this_report = get(report_label, model)
  ## check that the report label is of type "process"
  if (any(names(this_report) == "type")) {
    if (this_report$type != "process")
      stop(Paste("The report label '", report_label, "' is not a process. Please check that the correct report_label was specified."))
    if (!(this_report$sub_type %in% c("mortality_instantaneous", "mortality_instantaneous_retained")))
      stop(Paste("The report label '", report_label, "' is a process that should be type 'mortality_instantaneous' or 'mortality_instantaneous_retained'."))

  } else {
    print("multi iteration report found")
    muliple_iterations_in_a_report = TRUE
    N_runs = length(this_report)
    if (this_report$'1'$type != "process")
      stop(Paste("The report label '", report_label, "' is not a process. Please check that the correct report_label was specified."))
    if (!(this_report$'1'$sub_type %in% c("mortality_instantaneous", "mortality_instantaneous_retained")))
      stop(Paste("The report label '", report_label, "' is a process that should be type 'mortality_instantaneous' or 'mortality_instantaneous_retained'."))
  }

  if (!muliple_iterations_in_a_report) {
    ## only a single trajectory
    f_ndx = grepl(pattern = "fishing_pressure", names(this_report))
    start_index = as.numeric(regexpr(pattern = "\\[",text = names(this_report)[f_ndx])) + 1
    stop_index = as.numeric(regexpr(pattern = "\\]",text = names(this_report)[f_ndx])) - 1
    fisheries = substring(names(this_report)[f_ndx], start_index, last = stop_index)
    years =  this_report$year
    first_fishery = TRUE
    if(missing(col)) {
      palette(gray(seq(0.4,.90,len = length(fisheries))))
      Cols = palette()
    } else {
      Cols = col
    }
    ## create a plot
    for ( i in 1:length(fisheries)) {
      values = this_report[[which(f_ndx)[i]]]
      ## does the user want it plotted as percent B0

      if(missing(ylim)) {
        ymax = max(values) + quantile(values, 0.05)
        ylim = c(0, ymax)
      }
      if(missing(xlim))
        xlim = c(min(years) - 1, max(years) + 1)
      if(missing(ylab))
        ylab = "Fishing pressure (Exploitation Rate)"

      if(missing(xlab))
        xlab = "Years"
      if(missing(main))
        main = ""
      if (plot.it == TRUE && first_fishery) {
        plot(years, values, xlim = xlim, ylim = ylim, xlab = xlab, ylab = ylab, main = main, type = "o", col = Cols[i], ...)
        first_fishery = FALSE
      } else if (plot.it == TRUE && !first_fishery) {
        lines(years, values, col = Cols[i], type = "o", ...)
      } else {
        temp_DF = cbind(values,temp_DF)
      }
    }
    if (plot.it == FALSE) {
      colnames(temp_DF) = fisheries
    } else {
      legend('topright', legend = fisheries, col = Cols, lty = 1)
    }
  } else {
    ## Multiple trajectory's
    stop("This function does not take multiple inputs.")
  }

  if (plot.it == FALSE)
    return(temp_DF)
  invisible()
}

## method for class casal2TAB
#' @return \code{NULL}
#'
#' @rdname plot.pressure
#' @method plot.pressure casal2TAB
#' @export
"plot.pressure.casal2TAB" = function(model, report_label="", xlim, ylim, xlab, ylab, main, col, plot.it = T, ...) {
  ## check report label exists
  if (!report_label %in% names(model))
    stop(Paste("The report label '", report_label, "' was not found. The report labels available are: ", paste(names(model),collapse = ", ")))
  ## get the report out
  this_report = get(report_label, model)
  ## check that the report label is of type derived_quantity
  if (this_report$type != "process") {
    stop(Paste("The report label '", report_label, "' is not a derived quantity. Please check that the correct report_label was specified."))
  }
  if (!(this_report$process_type %in% c("mortality_instantaneous", "mortality_instantaneous_retained")) || is.null(this_report$process_type)) {
    stop(Paste("The process type in report '", report_label, "' is not 'mortality_instantaneous' or 'mortality_instantaneous_retained'. Please check that the correct report_label was specified."))
  }

  if (plot.it) {
    Labs = colnames(this_report$values)
    start_index = as.numeric(regexpr(pattern = "\\[",text =Labs)) + 1
    stop_index = as.numeric(regexpr(pattern = "\\]",text = Labs)) - 1
    Fisheries = unique(substring(Labs, start_index,last = stop_index))

    par(mfrow = c(1,length(Fisheries)))
    for (i in 1:length(Fisheries)) {
      ## pull out label and years
      ndx = grepl(pattern = Paste("fishing_pressure\\[", Fisheries[i]), x = Labs)
      this_ssb = this_report$values[,ndx]
      start_nd = as.numeric(regexpr(pattern = "\\]",text = colnames(this_ssb))) + 2
      years = as.numeric(substring(colnames(this_ssb),first = start_nd, last = nchar(colnames(this_ssb)) - 1))
      vals = apply(this_ssb, 2, quantile, c(0.025,0.5,0.975))
      plot(years,vals["50%",],ylim = c(0, max(vals)), ylab = "Fishing Pressure (exploitation rate)", xlab = "years", type = "l", main = Fisheries[i])
      polygon(x = c(years, rev(years)), y = c(vals["2.5%",], rev(vals["97.5%",])), col = "gray60")
      lines(years,vals["50%",], col = "red", lwd = 2)
    }
  } else {
    return(this_report$values)
  }
  invisible()
}
