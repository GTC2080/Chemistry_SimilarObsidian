declare module "jcampconverter" {
  interface ConvertOptions {
    noContour?: boolean;
    keepRecordsRegExp?: RegExp;
  }

  interface SpectrumDataBlock {
    x: number[];
    y: number[];
  }

  interface Spectrum {
    data: SpectrumDataBlock[];
    xUnits?: string;
    yUnits?: string;
    title?: string;
    dataType?: string;
  }

  interface ConvertResult {
    spectra: Spectrum[];
    title?: string;
  }

  export function convert(jcamp: string, options?: ConvertOptions): ConvertResult;
}
