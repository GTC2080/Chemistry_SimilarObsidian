import type { Config, Data, Layout } from "plotly.js";
import createPlotlyComponent from "react-plotly.js/factory";
import PlotlyBasic from "plotly.js-basic-dist-min";

const Plot = createPlotlyComponent(PlotlyBasic as any);

interface PlotlySpectrumChartProps {
  traces: Data[];
  layout: Partial<Layout>;
  config: Partial<Config>;
}

export default function PlotlySpectrumChart({
  traces,
  layout,
  config,
}: PlotlySpectrumChartProps) {
  return (
    <Plot
      data={traces}
      layout={layout}
      config={config}
      useResizeHandler
      style={{ width: "100%", height: "100%" }}
    />
  );
}
