#' Utility extract function
#'
#' @author Dan Fu
#' @description 
#' create a matrix, does not expect header values.
#' @keywords internal
#'
"make.matrix" <-
function(lines)
{
  columns <- string.to.vector.of.words(lines[1])
  if(length(lines) < 2) 
    return(NA)
  data <- matrix(0, length(lines), length(columns))
  for(i in 1:length(lines)) {
      line = string.to.vector.of.numbers(lines[i])
      if (length(line) != length(columns)) {
          stop(paste(lines[i],"is not the same length as",lines[1]))
      }
    data[i,  ] <- line
  }
  #colnames(data) <- columns
  data
}

