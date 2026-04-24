declare module "smiles-drawer" {
  export class SmiDrawer {
    constructor(options?: Record<string, any>);
    draw(tree: any, target: HTMLCanvasElement | HTMLElement, theme?: string): void;
    static parse(smiles: string, onSuccess: (tree: any) => void, onError: (err: any) => void): void;
  }
  export default SmiDrawer;
}
