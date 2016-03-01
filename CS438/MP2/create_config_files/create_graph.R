rm(list=ls())
unlink(list.files("cost_files", full.names=T))

n = 61
max.distance = 10
one.duplicate = 5
NA.length = 200

A = matrix(sample(c(rep(NA, NA.length), rep(1, one.duplicate), 1:max.distance), size=n*n, replace=T), nrow=n, ncol=n)
diag(A) = NA
A[lower.tri(A)] = t(A)[lower.tri(A)]

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
plot.igraph(graph, edge.label=E(graph)$weight)
