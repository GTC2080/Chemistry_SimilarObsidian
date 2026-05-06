import { describe, expect, it } from "vitest";
import { createPdfTextSearchCache, type PdfTextDocument } from "./pdfTextSearchCache";

function makeDoc() {
  const textCalls = new Map<number, number>();
  const doc: PdfTextDocument = {
    numPages: 2,
    async getPage(pageNumber: number) {
      return {
        async getTextContent() {
          textCalls.set(pageNumber, (textCalls.get(pageNumber) ?? 0) + 1);
          return {
            items: [
              {
                str: pageNumber === 1 ? "Alpha bond" : "Beta spectrum",
                transform: [1, 0, 0, 10, 5, 20],
                width: 80,
                height: 10,
              },
            ],
          };
        },
        getViewport() {
          return { width: 100, height: 200 };
        },
      };
    },
  };
  return { doc, textCalls };
}

describe("PDF text search cache", () => {
  it("reuses extracted page text across searches", async () => {
    const { doc, textCalls } = makeDoc();
    const cache = createPdfTextSearchCache(doc);

    await expect(cache.search("Alpha")).resolves.toHaveLength(1);
    await expect(cache.search("Beta")).resolves.toHaveLength(1);

    expect(textCalls.get(1)).toBe(1);
    expect(textCalls.get(2)).toBe(1);
  });

  it("returns an empty result for stale searches superseded by a newer query", async () => {
    let releaseFirstPage: () => void = () => {};
    const firstPageGate = new Promise<void>((resolve) => { releaseFirstPage = resolve; });
    const doc: PdfTextDocument = {
      numPages: 1,
      async getPage() {
        return {
          async getTextContent() {
            await firstPageGate;
            return {
              items: [
                { str: "Alpha", transform: [1, 0, 0, 10, 5, 20], width: 80, height: 10 },
              ],
            };
          },
          getViewport() {
            return { width: 100, height: 200 };
          },
        };
      },
    };
    const cache = createPdfTextSearchCache(doc);

    const first = cache.search("Alpha");
    const second = cache.search("Beta");
    releaseFirstPage();

    await expect(first).resolves.toEqual([]);
    await expect(second).resolves.toEqual([]);
  });
});
