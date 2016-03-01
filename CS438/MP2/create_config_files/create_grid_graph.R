rm(list=ls())
unlink(list.files("cost_files", full.names=T))

n = 225
nrow = sqrt(n)
ncol = nrow
max.distance = 10
one.duplicate = 5

A = matrix(rep(NA, n*n), nrow=n, ncol=n)
sapply(0:(nrow-1), function(i) {
    sapply(0:(ncol-2), function(j) {
        A[i*ncol+j+1, i*ncol+j+2] <<- sample(c(rep(1, one.duplicate), 1:max.distance), size=1)
        A[i*ncol+j+2, i*ncol+j+1] <<- A[i*ncol+j+1, i*ncol+j+2]
    })
})
sapply(0:(ncol-1), function(j) {
    sapply(0:(nrow-2), function(i) {
        A[i*ncol+j+1, (i+1)*ncol+j+1] <<- sample(c(rep(1, one.duplicate), 1:max.distance), size=1)
        A[(i+1)*ncol+j+1, i*ncol+j+1] <<- A[i*ncol+j+1, (i+1)*ncol+j+1]
    })
})

sapply(1:n, function(x) {
    out = file(paste("cost_files/costs", x - 1, sep="_"), "w")
    sapply(which(!is.na(A[x, ]) & A[x, ] != 1), function(y) {
        cat(paste(y - 1, " ", A[x, y], "\n", sep=""), file=out)
    })
    close(out)
})

out = file("topology", "w")
sapply(1:(n - 1), function(x) {
    sapply(which(!is.na(A[x, (x + 1):n])) + x, function(y) {
        cat(paste(x - 1, " ", y - 1, "\n", sep=""), file=out)
    })
})
close(out)

A[is.na(A)] = 0
graph = graph.adjacency(A, mode="undirected", weighted=T, diag=F)
V(graph)$name = 0:(nrow(A) - 1)
plot.igraph(graph, edge.label=E(graph)$weight, vertex.size=3)
