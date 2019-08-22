from tools import cudaMethod


class System:
    useGpu = True
    maxGenerationCount = 1000
    threadsPerBlock = 32
    blocksPerGrid = (maxGenerationCount() +
                     (threadsPerBlock - 1)) // threadsPerBlock


class Substrate:
    dimension = 2
    inputSize = 3
    outputSize = 1
    layersCount = 5
    layerSize = 5

    @cudaMethod()
    @staticmethod
    def getSubstrate():
        res = [[(0, j) for j in range(Substrate.inputSize)]]
        for i in range(Substrate.layersCount):
            res.append(
                [(i+1, j) for j in range(Substrate.layerSize)])
        res.append([(Substrate.layersCount+1, j)
                    for j in range(Substrate.outputSize)])
        return res


class Params:
    populationSize = 1000
    weightThreshold = 0.05
